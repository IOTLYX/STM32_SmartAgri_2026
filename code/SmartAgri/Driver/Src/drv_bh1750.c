#include "drv_bh1750.h"
#include <string.h>

/* ====== BH1750 命令 ====== */
#define BH1750_CMD_POWER_ON      0x01
#define BH1750_CMD_RESET         0x07

/* One-Time High Resolution Mode (1 lx resolution, typical 120ms) */
#define BH1750_CMD_ONE_HRES      0x20

/* ====== 参数可调 ====== */
#define BH1750_I2C_TIMEOUT_MS    20
#define BH1750_CONV_WAIT_MS      180   // 等待转换完成（保守一点，避免读到旧值）

typedef enum {
    ST_IDLE = 0,
    ST_WAITING,
} bh1750_state_t;

static I2C_HandleTypeDef *s_hi2c = NULL;
static uint8_t s_addr7 = 0;

static bh1750_state_t s_st = ST_IDLE;
static uint32_t s_t0 = 0;

static bh1750_data_t s_dat = {0};
static bh1750_th_t s_th = {0};

/* ====== 内部I2C封装（HAL用8bit地址=7bit<<1） ====== */
static HAL_StatusTypeDef _i2c_wr(uint8_t cmd)
{
    uint8_t a8 = (uint8_t)(s_addr7 << 1);
    return HAL_I2C_Master_Transmit(s_hi2c, a8, &cmd, 1, BH1750_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef _i2c_rd2(uint8_t buf2[2])
{
    uint8_t a8 = (uint8_t)(s_addr7 << 1);
    return HAL_I2C_Master_Receive(s_hi2c, a8, buf2, 2, BH1750_I2C_TIMEOUT_MS);
}

/* raw -> lux：datasheet 常见换算 lux = raw / 1.2 */
static uint32_t _raw_to_lux(uint16_t raw)
{
    /* lux = raw / 1.2 = raw * 10 / 12 */
    return (uint32_t)raw * 10u / 12u;
}

void drv_bh1750_init(I2C_HandleTypeDef *hi2c, uint8_t addr7)
{
    s_hi2c = hi2c;
    s_addr7 = addr7;

    s_st = ST_IDLE;
    s_t0 = 0;

    memset(&s_dat, 0, sizeof(s_dat));
    memset(&s_th,  0, sizeof(s_th));

    if (!s_hi2c) return;

    /* 上电 + reset（非必须，但建议做一下） */
    if (_i2c_wr(BH1750_CMD_POWER_ON) != HAL_OK) {
        s_dat.ok = 0;
        return;
    }
    (void)_i2c_wr(BH1750_CMD_RESET);

    s_dat.ok = 1;
}

bool drv_bh1750_sample(void)
{
    if (!s_hi2c) return false;

    /* 如果I2C忙/挂死，这里直接报错（你也可以加I2C恢复逻辑） */
    if (HAL_I2C_GetState(s_hi2c) != HAL_I2C_STATE_READY) {
        s_dat.ok = 0;
        return false;
    }

    if (s_st == ST_IDLE) {
        /* 发起一次测量（one-time），然后等待转换完成 */
        if (_i2c_wr(BH1750_CMD_ONE_HRES) != HAL_OK) {
            s_dat.ok = 0;
            return false;
        }
        s_t0 = HAL_GetTick();
        s_st = ST_WAITING;
        return false;
    }

    /* 等待转换时间到 */
    if (s_st == ST_WAITING) {
        uint32_t now = HAL_GetTick();
        if ((int32_t)(now - s_t0) < (int32_t)BH1750_CONV_WAIT_MS) {
            return false;
        }

        uint8_t buf[2];
        if (_i2c_rd2(buf) != HAL_OK) {
            s_dat.ok = 0;
            s_st = ST_IDLE;
            return false;
        }

        uint16_t raw = (uint16_t)((buf[0] << 8) | buf[1]);
        s_dat.lux  = _raw_to_lux(raw);
        s_dat.ts_ms = now;
        s_dat.ok = 1;

        s_st = ST_IDLE;
        return true; /* ★本次更新成功 */
    }

    /* 不该到这里 */
    s_st = ST_IDLE;
    return false;
}

void drv_bh1750_get(bh1750_data_t *out)
{
    if (!out) return;
    *out = s_dat;
}

void drv_bh1750_set_threshold(const bh1750_th_t *th)
{
    if (!th) {
        memset(&s_th, 0, sizeof(s_th));
        return;
    }
    s_th = *th;
}

uint8_t drv_bh1750_alarm_eval(void)
{
    uint8_t a = BH1750_ALM_NONE;

    if (!s_dat.ok) {
        a |= BH1750_ALM_SENSOR;
        return a;
    }

    if (s_th.en_lux_lo && s_dat.lux <= s_th.lux_lo) a |= BH1750_ALM_LUX_LO;
    if (s_th.en_lux_hi && s_dat.lux >= s_th.lux_hi) a |= BH1750_ALM_LUX_HI;

    return a;
}
