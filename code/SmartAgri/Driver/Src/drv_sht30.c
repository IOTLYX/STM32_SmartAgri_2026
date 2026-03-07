#include "drv_sht30.h"
#include <string.h>

/* ===== 可调参数 ===== */
#define SHT30_I2C_TIMEOUT_MS    20
#define SHT30_CONV_WAIT_MS      20   // 单次测量等待时间（高精度保守20ms）

/* 单次测量：High repeatability, clock stretching disabled */
#define SHT30_CMD_MEAS_HIREP_NOCS_MSB   0x24
#define SHT30_CMD_MEAS_HIREP_NOCS_LSB   0x00

typedef enum {
    ST_IDLE = 0,
    ST_WAITING,
} sht30_state_t;

static I2C_HandleTypeDef *s_hi2c = NULL;
static uint8_t s_addr7 = 0;

static sht30_state_t s_st = ST_IDLE;
static uint32_t s_t0 = 0;

static sht30_data_t s_dat = {0};
static sht30_th_t   s_th  = {0};

static HAL_StatusTypeDef _i2c_wr2(uint8_t b0, uint8_t b1)
{
    uint8_t a8 = (uint8_t)(s_addr7 << 1);
    uint8_t buf[2] = { b0, b1 };
    return HAL_I2C_Master_Transmit(s_hi2c, a8, buf, 2, SHT30_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef _i2c_rd6(uint8_t buf6[6])
{
    uint8_t a8 = (uint8_t)(s_addr7 << 1);
    return HAL_I2C_Master_Receive(s_hi2c, a8, buf6, 6, SHT30_I2C_TIMEOUT_MS);
}

/* CRC8: polynomial 0x31, init 0xFF（SHT3x标准） */
static uint8_t _crc8_sht3x(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x80) crc = (uint8_t)((crc << 1) ^ 0x31);
            else            crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* rawT -> temp_x10: T(°C) = -45 + 175 * raw / 65535 */
static int16_t _raw_to_temp_x10(uint16_t raw)
{
    /* 用整数算：T_x10 = (-45*10) + (175*10*raw)/65535 */
    int32_t t = -450;
    t += (int32_t)(1750L * (int32_t)raw) / 65535L;
    return (int16_t)t;
}

/* rawRH -> hum_x10: RH(%) = 100 * raw / 65535 */
static uint16_t _raw_to_hum_x10(uint16_t raw)
{
    /* RH_x10 = (100*10*raw)/65535 */
    uint32_t h = (uint32_t)(1000UL * (uint32_t)raw) / 65535UL;
    if (h > 1000UL) h = 1000UL;
    return (uint16_t)h;
}

void drv_sht30_init(I2C_HandleTypeDef *hi2c, uint8_t addr7)
{
    s_hi2c = hi2c;
    s_addr7 = addr7;

    s_st = ST_IDLE;
    s_t0 = 0;

    memset(&s_dat, 0, sizeof(s_dat));
    memset(&s_th,  0, sizeof(s_th));

    if (!s_hi2c) return;
    s_dat.ok = 1;
}

bool drv_sht30_sample(void)
{
    if (!s_hi2c) return false;

    if (HAL_I2C_GetState(s_hi2c) != HAL_I2C_STATE_READY) {
        s_dat.ok = 0;
        return false;
    }

    if (s_st == ST_IDLE) {
        /* 发起单次测量 */
        if (_i2c_wr2(SHT30_CMD_MEAS_HIREP_NOCS_MSB, SHT30_CMD_MEAS_HIREP_NOCS_LSB) != HAL_OK) {
            s_dat.ok = 0;
            return false;
        }
        s_t0 = HAL_GetTick();
        s_st = ST_WAITING;
        return false;
    }

    /* 等待测量完成 */
    if (s_st == ST_WAITING) {
        uint32_t now = HAL_GetTick();
        if ((int32_t)(now - s_t0) < (int32_t)SHT30_CONV_WAIT_MS) {
            return false;
        }

        uint8_t buf[6];
        if (_i2c_rd6(buf) != HAL_OK) {
            s_dat.ok = 0;
            s_st = ST_IDLE;
            return false;
        }

        /* CRC校验：buf[0..1]温度, buf[2]CRC; buf[3..4]湿度, buf[5]CRC */
        uint8_t crc_t = _crc8_sht3x(&buf[0], 2);
        uint8_t crc_h = _crc8_sht3x(&buf[3], 2);
        if (crc_t != buf[2] || crc_h != buf[5]) {
            s_dat.ok = 0;
            s_st = ST_IDLE;
            return false;
        }

        uint16_t rawT  = (uint16_t)((buf[0] << 8) | buf[1]);
        uint16_t rawRH = (uint16_t)((buf[3] << 8) | buf[4]);

        s_dat.temp_x10 = _raw_to_temp_x10(rawT);
        s_dat.hum_x10  = _raw_to_hum_x10(rawRH);
        s_dat.ts_ms    = now;
        s_dat.ok       = 1;

        s_st = ST_IDLE;
        return true;
    }

    s_st = ST_IDLE;
    return false;
}

void drv_sht30_get(sht30_data_t *out)
{
    if (!out) return;
    *out = s_dat;
}

void drv_sht30_set_threshold(const sht30_th_t *th)
{
    if (!th) {
        memset(&s_th, 0, sizeof(s_th));
        return;
    }
    s_th = *th;
}

uint8_t drv_sht30_alarm_eval(void)
{
    uint8_t a = SHT30_ALM_NONE;

    if (!s_dat.ok) {
        a |= SHT30_ALM_SENSOR;
        return a;
    }

    if (s_th.en_temp_hi && s_dat.temp_x10 >= s_th.temp_hi_x10) a |= SHT30_ALM_TEMP_HI;
    if (s_th.en_temp_lo && s_dat.temp_x10 <= s_th.temp_lo_x10) a |= SHT30_ALM_TEMP_LO;
    if (s_th.en_hum_hi  && s_dat.hum_x10  >= s_th.hum_hi_x10)  a |= SHT30_ALM_HUM_HI;
    if (s_th.en_hum_lo  && s_dat.hum_x10  <= s_th.hum_lo_x10)  a |= SHT30_ALM_HUM_LO;

    return a;
}
