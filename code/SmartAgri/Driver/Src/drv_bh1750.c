#include "drv_bh1750.h"
#include <string.h>

/* ====== BH1750 命令 ====== */
#define BH1750_CMD_POWER_ON      0x01    /* 上电命令 */
#define BH1750_CMD_RESET         0x07    /* 数据寄存器复位命令 */

/* One-Time High Resolution Mode（1 lx 分辨率，典型转换时间 120 ms） */
#define BH1750_CMD_ONE_HRES      0x20

/* ====== 参数可调 ====== */
#define BH1750_I2C_TIMEOUT_MS    20      /* I2C 通信超时时间，单位 ms */
#define BH1750_CONV_WAIT_MS      180     /* 等待转换完成时间，单位 ms，取保守值避免读到旧数据 */

/**
 * @brief BH1750 内部状态机状态定义
 */
typedef enum
{
    ST_IDLE = 0,   /* 空闲状态，可发起新一轮测量 */
    ST_WAITING,    /* 已发起测量，等待转换完成 */
} bh1750_state_t;

/* BH1750 所使用的 I2C 句柄 */
static I2C_HandleTypeDef *s_hi2c = NULL;

/* BH1750 设备 7bit 地址 */
static uint8_t s_addr7 = 0;

/* 当前采样状态机状态 */
static bh1750_state_t s_st = ST_IDLE;

/* 本轮采样起始时间戳 */
static uint32_t s_t0 = 0;

/* 最近一次采样数据缓存 */
static bh1750_data_t s_dat = {0};

/* 当前告警阈值配置 */
static bh1750_th_t s_th = {0};

/**
 * @brief I2C 写 1 字节命令
 *
 * HAL 的主机收发接口使用 8bit 地址，因此这里需要将 7bit 地址左移 1 位。
 *
 * @param cmd 待写入的命令字节
 * @return HAL I2C 操作状态
 */
static HAL_StatusTypeDef _i2c_wr(uint8_t cmd)
{
    uint8_t a8 = (uint8_t)(s_addr7 << 1);
    return HAL_I2C_Master_Transmit(s_hi2c, a8, &cmd, 1, BH1750_I2C_TIMEOUT_MS);
}

/**
 * @brief I2C 读取 2 字节数据
 *
 * 用于读取 BH1750 测量结果高低字节。
 *
 * @param buf2 接收缓冲区，长度必须至少为 2
 * @return HAL I2C 操作状态
 */
static HAL_StatusTypeDef _i2c_rd2(uint8_t buf2[2])
{
    uint8_t a8 = (uint8_t)(s_addr7 << 1);
    return HAL_I2C_Master_Receive(s_hi2c, a8, buf2, 2, BH1750_I2C_TIMEOUT_MS);
}

/**
 * @brief 将原始测量值换算为 lux
 *
 * 根据数据手册常见换算关系：
 * lux = raw / 1.2
 *
 * 为避免浮点运算，这里改写为整数形式：
 * lux = raw * 10 / 12
 *
 * @param raw 原始 16bit 测量值
 * @return 换算后的光照强度，单位 lux
 */
static uint32_t _raw_to_lux(uint16_t raw)
{
    return (uint32_t)raw * 10u / 12u;
}

/**
 * @brief 初始化 BH1750 驱动
 *
 * 保存 I2C 句柄和设备地址，清空内部状态与缓存，
 * 并执行一次上电和复位操作。
 *
 * @param hi2c  I2C 句柄
 * @param addr7 设备 7bit 地址
 */
void drv_bh1750_init(I2C_HandleTypeDef *hi2c, uint8_t addr7)
{
    s_hi2c = hi2c;
    s_addr7 = addr7;

    /* 重置状态机 */
    s_st = ST_IDLE;
    s_t0 = 0;

    /* 清空数据缓存和阈值配置 */
    memset(&s_dat, 0, sizeof(s_dat));
    memset(&s_th,  0, sizeof(s_th));

    if (!s_hi2c)
    {
        return;
    }

    /* 上电 + 复位，复位不是必须，但建议初始化时执行一次 */
    if (_i2c_wr(BH1750_CMD_POWER_ON) != HAL_OK)
    {
        s_dat.ok = 0;
        return;
    }

    (void)_i2c_wr(BH1750_CMD_RESET);

    s_dat.ok = 1;
}

/**
 * @brief 触发并完成一次 BH1750 采样
 *
 * 采用非阻塞状态机方式：
 * 1. 空闲态发送一次测量命令
 * 2. 等待转换完成
 * 3. 读取结果并更新缓存
 *
 * @return true  本次调用成功完成一次采样并更新缓存
 * @return false 当前仍在等待、初始化未完成或采样失败
 */
bool drv_bh1750_sample(void)
{
    if (!s_hi2c)
    {
        return false;
    }

    /* 若 I2C 外设当前不空闲，则认为本次采样失败 */
    if (HAL_I2C_GetState(s_hi2c) != HAL_I2C_STATE_READY)
    {
        s_dat.ok = 0;
        return false;
    }

    if (s_st == ST_IDLE)
    {
        /* 发起一次单次高精度测量，然后进入等待状态 */
        if (_i2c_wr(BH1750_CMD_ONE_HRES) != HAL_OK)
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

        /* 转换时间未到，继续等待 */
        if ((int32_t)(now - s_t0) < (int32_t)BH1750_CONV_WAIT_MS)
        {
            return false;
        }

        /* 转换完成后读取 2 字节原始测量值 */
        uint8_t buf[2];
        if (_i2c_rd2(buf) != HAL_OK)
        {
            s_dat.ok = 0;
            s_st = ST_IDLE;
            return false;
        }

        /* 拼接高低字节得到原始值 */
        uint16_t raw = (uint16_t)((buf[0] << 8) | buf[1]);

        /* 换算 lux 并更新缓存 */
        s_dat.lux = _raw_to_lux(raw);
        s_dat.ts_ms = now;
        s_dat.ok = 1;

        /* 一次采样流程结束，回到空闲态 */
        s_st = ST_IDLE;
        return true;
    }

    /* 异常保护：若状态机进入未知状态，则强制复位到空闲态 */
    s_st = ST_IDLE;
    return false;
}

/**
 * @brief 获取最近一次 BH1750 数据缓存
 *
 * @param out 输出数据指针
 */
void drv_bh1750_get(bh1750_data_t *out)
{
    if (!out)
    {
        return;
    }

    *out = s_dat;
}

/**
 * @brief 设置 BH1750 阈值配置
 *
 * 若传入空指针，则清空全部阈值配置。
 *
 * @param th 阈值配置指针
 */
void drv_bh1750_set_threshold(const bh1750_th_t *th)
{
    if (!th)
    {
        memset(&s_th, 0, sizeof(s_th));
        return;
    }

    s_th = *th;
}

/**
 * @brief 根据当前缓存数据和阈值配置计算告警位
 *
 * 告警判断逻辑：
 * - 采样无效：置传感器异常告警
 * - 光照低于低阈值：置低光照告警
 * - 光照高于高阈值：置高光照告警
 *
 * @return 告警位组合值
 */
uint8_t drv_bh1750_alarm_eval(void)
{
    uint8_t a = BH1750_ALM_NONE;

    /* 数据无效时直接返回传感器异常告警 */
    if (!s_dat.ok)
    {
        a |= BH1750_ALM_SENSOR;
        return a;
    }

    if (s_th.en_lux_lo && s_dat.lux <= s_th.lux_lo)
    {
        a |= BH1750_ALM_LUX_LO;
    }

    if (s_th.en_lux_hi && s_dat.lux >= s_th.lux_hi)
    {
        a |= BH1750_ALM_LUX_HI;
    }

    return a;
}