#include "drv_sht30.h"
#include <string.h>

/* ==================== 可调参数 ==================== */

/** @brief I2C 通信超时时间，单位 ms */
#define SHT30_I2C_TIMEOUT_MS    20

/** @brief 单次测量等待时间，单位 ms（高精度模式保守取 20ms） */
#define SHT30_CONV_WAIT_MS      20

/* ==================== SHT30 命令字 ==================== */

/** @brief 单次测量命令高字节：高重复精度，关闭时钟拉伸 */
#define SHT30_CMD_MEAS_HIREP_NOCS_MSB   0x24

/** @brief 单次测量命令低字节：高重复精度，关闭时钟拉伸 */
#define SHT30_CMD_MEAS_HIREP_NOCS_LSB   0x00

/**
 * @brief SHT30 采样状态
 */
typedef enum
{
    ST_IDLE = 0,   /**< 空闲态，可发起新测量 */
    ST_WAITING,    /**< 等待测量完成 */
} sht30_state_t;

/** @brief SHT30 所在 I2C 句柄 */
static I2C_HandleTypeDef *s_hi2c = NULL;

/** @brief SHT30 7 位从机地址 */
static uint8_t s_addr7 = 0;

/** @brief 当前采样状态 */
static sht30_state_t s_st = ST_IDLE;

/** @brief 本轮测量起始时刻，单位 ms */
static uint32_t s_t0 = 0;

/** @brief 当前传感器数据缓存 */
static sht30_data_t s_dat = {0};

/** @brief 当前阈值配置 */
static sht30_th_t s_th = {0};

/**
 * @brief 向 SHT30 写入 2 字节命令
 * @param b0 第 1 个字节
 * @param b1 第 2 个字节
 * @return HAL 状态码
 */
static HAL_StatusTypeDef _i2c_wr2(uint8_t b0, uint8_t b1)
{
    uint8_t a8 = (uint8_t)(s_addr7 << 1);
    uint8_t buf[2] = {b0, b1};

    return HAL_I2C_Master_Transmit(s_hi2c, a8, buf, 2, SHT30_I2C_TIMEOUT_MS);
}

/**
 * @brief 从 SHT30 读取 6 字节测量结果
 * @param buf6 读取缓冲区，长度必须为 6 字节
 * @return HAL 状态码
 */
static HAL_StatusTypeDef _i2c_rd6(uint8_t buf6[6])
{
    uint8_t a8 = (uint8_t)(s_addr7 << 1);

    return HAL_I2C_Master_Receive(s_hi2c, a8, buf6, 6, SHT30_I2C_TIMEOUT_MS);
}

/**
 * @brief 计算 SHT3x 数据 CRC8
 * @param data 数据指针
 * @param len 数据长度
 * @return CRC8 校验值
 * @note 多项式 0x31，初值 0xFF
 */
static uint8_t _crc8_sht3x(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;

    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
        {
            if (crc & 0x80)
            {
                crc = (uint8_t)((crc << 1) ^ 0x31);
            }
            else
            {
                crc = (uint8_t)(crc << 1);
            }
        }
    }

    return crc;
}

/**
 * @brief 将温度原始值转换为 0.1℃ 定点数
 * @param raw 温度原始值
 * @return 温度值，单位 0.1℃
 * @note 转换公式：T(°C) = -45 + 175 * raw / 65535
 */
static int16_t _raw_to_temp_x10(uint16_t raw)
{
    /* T_x10 = -450 + (1750 * raw) / 65535 */
    int32_t t = -450;
    t += (int32_t)(1750L * (int32_t)raw) / 65535L;

    return (int16_t)t;
}

/**
 * @brief 将湿度原始值转换为 0.1%RH 定点数
 * @param raw 湿度原始值
 * @return 湿度值，单位 0.1%RH
 * @note 转换公式：RH(%) = 100 * raw / 65535
 */
static uint16_t _raw_to_hum_x10(uint16_t raw)
{
    /* RH_x10 = (1000 * raw) / 65535 */
    uint32_t h = (uint32_t)(1000UL * (uint32_t)raw) / 65535UL;

    if (h > 1000UL)
    {
        h = 1000UL;
    }

    return (uint16_t)h;
}

/**
 * @brief 初始化 SHT30 驱动
 * @param hi2c I2C 句柄
 * @param addr7 SHT30 7 位从机地址
 * @return 无
 */
void drv_sht30_init(I2C_HandleTypeDef *hi2c, uint8_t addr7)
{
    s_hi2c = hi2c;
    s_addr7 = addr7;

    s_st = ST_IDLE;
    s_t0 = 0;

    memset(&s_dat, 0, sizeof(s_dat));
    memset(&s_th, 0, sizeof(s_th));

    if (!s_hi2c)
    {
        return;
    }

    s_dat.ok = 1;
}

/**
 * @brief 执行一次非阻塞采样流程
 * @param 无
 * @return true  本次成功得到新数据
 * @return false 本次未完成采样或采样失败
 * @note 采用两阶段状态机：
 *       1. 空闲态发送单次测量命令
 *       2. 等待转换完成后读取并校验数据
 */
bool drv_sht30_sample(void)
{
    if (!s_hi2c)
    {
        return false;
    }

    /* I2C 忙时不发起新事务 */
    if (HAL_I2C_GetState(s_hi2c) != HAL_I2C_STATE_READY)
    {
        s_dat.ok = 0;
        return false;
    }

    if (s_st == ST_IDLE)
    {
        /* 发起单次测量 */
        if (_i2c_wr2(SHT30_CMD_MEAS_HIREP_NOCS_MSB, SHT30_CMD_MEAS_HIREP_NOCS_LSB) != HAL_OK)
        {
            s_dat.ok = 0;
            return false;
        }

        s_t0 = HAL_GetTick();
        s_st = ST_WAITING;
        return false;
    }

    if (s_st == ST_WAITING)
    {
        uint32_t now = HAL_GetTick();

        /* 测量时间未到，继续等待 */
        if ((int32_t)(now - s_t0) < (int32_t)SHT30_CONV_WAIT_MS)
        {
            return false;
        }

        uint8_t buf[6];

        /* 读取温湿度原始数据 + CRC */
        if (_i2c_rd6(buf) != HAL_OK)
        {
            s_dat.ok = 0;
            s_st = ST_IDLE;
            return false;
        }

        /* 数据格式：
         * buf[0..1]：温度原始值
         * buf[2]   ：温度 CRC
         * buf[3..4]：湿度原始值
         * buf[5]   ：湿度 CRC
         */
        uint8_t crc_t = _crc8_sht3x(&buf[0], 2);
        uint8_t crc_h = _crc8_sht3x(&buf[3], 2);

        if ((crc_t != buf[2]) || (crc_h != buf[5]))
        {
            s_dat.ok = 0;
            s_st = ST_IDLE;
            return false;
        }

        uint16_t rawT  = (uint16_t)((buf[0] << 8) | buf[1]);
        uint16_t rawRH = (uint16_t)((buf[3] << 8) | buf[4]);

        /* 原始值转换为工程单位 */
        s_dat.temp_x10 = _raw_to_temp_x10(rawT);
        s_dat.hum_x10  = _raw_to_hum_x10(rawRH);
        s_dat.ts_ms    = now;
        s_dat.ok       = 1;

        s_st = ST_IDLE;
        return true;
    }

    /* 异常状态兜底恢复 */
    s_st = ST_IDLE;
    return false;
}

/**
 * @brief 获取当前缓存的传感器数据
 * @param out 输出数据指针
 * @return 无
 */
void drv_sht30_get(sht30_data_t *out)
{
    if (!out)
    {
        return;
    }

    *out = s_dat;
}

/**
 * @brief 设置告警阈值
 * @param th 阈值配置指针，传 NULL 表示清除阈值配置
 * @return 无
 */
void drv_sht30_set_threshold(const sht30_th_t *th)
{
    if (!th)
    {
        memset(&s_th, 0, sizeof(s_th));
        return;
    }

    s_th = *th;
}

/**
 * @brief 根据当前数据和阈值计算告警标志
 * @param 无
 * @return 告警位掩码，参考 SHT30_ALM_xxx 定义
 */
uint8_t drv_sht30_alarm_eval(void)
{
    uint8_t a = SHT30_ALM_NONE;

    /* 传感器当前无有效数据时，直接上报传感器故障 */
    if (!s_dat.ok)
    {
        a |= SHT30_ALM_SENSOR;
        return a;
    }

    if (s_th.en_temp_hi && (s_dat.temp_x10 >= s_th.temp_hi_x10))
    {
        a |= SHT30_ALM_TEMP_HI;
    }

    if (s_th.en_temp_lo && (s_dat.temp_x10 <= s_th.temp_lo_x10))
    {
        a |= SHT30_ALM_TEMP_LO;
    }

    if (s_th.en_hum_hi && (s_dat.hum_x10 >= s_th.hum_hi_x10))
    {
        a |= SHT30_ALM_HUM_HI;
    }

    if (s_th.en_hum_lo && (s_dat.hum_x10 <= s_th.hum_lo_x10))
    {
        a |= SHT30_ALM_HUM_LO;
    }

    return a;
}