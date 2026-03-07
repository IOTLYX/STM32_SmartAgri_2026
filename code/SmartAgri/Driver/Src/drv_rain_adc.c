#include "drv_rain_adc.h"
#include <string.h>

/* ===== 参数 ===== */
#define RAIN_SAMPLES_PER       5
#define RAIN_POLL_TIMEOUT_MS   3

static ADC_HandleTypeDef *s_hadc = NULL;
static rain_data_t s_dat = {0};
static rain_cfg_t  s_cfg = {0};
static bool s_is_raining = false;

static uint8_t _clamp_u8(int32_t v, uint8_t lo, uint8_t hi)
{
    if (v < (int32_t)lo) return lo;
    if (v > (int32_t)hi) return hi;
    return (uint8_t)v;
}

/* raw -> pct（0~100，越大越湿/有雨）
 * 兼容两种方向：
 * - 常见：湿时 raw 更小（raw_wet < raw_dry）
 * - 也可能：湿时 raw 更大（raw_wet > raw_dry）
 */
static uint8_t _raw_to_pct(uint16_t raw, uint16_t raw_dry, uint16_t raw_wet)
{
    if (raw_dry == raw_wet) return 0;

    int32_t pct;
    if (raw_wet > raw_dry) {
        /* 湿时更大：pct = (raw-raw_dry)/(raw_wet-raw_dry) */
        pct = ((int32_t)raw - (int32_t)raw_dry) * 100
            / ((int32_t)raw_wet - (int32_t)raw_dry);
    } else {
        /* 湿时更小：pct = (raw_dry-raw)/(raw_dry-raw_wet) */
        pct = ((int32_t)raw_dry - (int32_t)raw) * 100
            / ((int32_t)raw_dry - (int32_t)raw_wet);
    }
    return _clamp_u8(pct, 0, 100);
}

static bool _select_channel_in9(void)
{
    if (!s_hadc) return false;

    ADC_ChannelConfTypeDef cfg = {0};
    cfg.Channel      = ADC_CHANNEL_9;            // ADC1_IN9
    cfg.Rank         = ADC_REGULAR_RANK_1;
    cfg.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;

    return (HAL_ADC_ConfigChannel(s_hadc, &cfg) == HAL_OK);
}

void drv_rain_init(ADC_HandleTypeDef *hadc)
{
    s_hadc = hadc;
    memset(&s_dat, 0, sizeof(s_dat));
    memset(&s_cfg, 0, sizeof(s_cfg));

    /* 默认标定（你后面用实测覆盖） */
    s_cfg.raw_dry = 3500;
    s_cfg.raw_wet = 1500;

    /* 默认阈值：>=60% 认为下雨；<=45% 解除 */
    s_cfg.rain_on_pct  = 60;
    s_cfg.rain_off_pct = 45;
    s_cfg.en_hysteresis = 1;

    s_is_raining = false;
}

void drv_rain_set_cfg(const rain_cfg_t *cfg)
{
    if (!cfg) return;
    s_cfg = *cfg;

    s_cfg.rain_on_pct  = _clamp_u8(s_cfg.rain_on_pct,  0, 100);
    s_cfg.rain_off_pct = _clamp_u8(s_cfg.rain_off_pct, 0, 100);

    if (s_cfg.en_hysteresis && s_cfg.rain_on_pct < s_cfg.rain_off_pct) {
        /* 如果用户写反了（on < off），就交换一下 */
        uint8_t t = s_cfg.rain_on_pct;
        s_cfg.rain_on_pct = s_cfg.rain_off_pct;
        s_cfg.rain_off_pct = t;
    }
}

bool drv_rain_sample(void)
{
    if (!s_hadc) { s_dat.ok = 0; return false; }

    /* ★关键：采样前切到 IN9（否则会被土壤IN1影响） */
    if (!_select_channel_in9()) { s_dat.ok = 0; return false; }

    uint32_t sum = 0;
    for (int i = 0; i < RAIN_SAMPLES_PER; i++) {
        if (HAL_ADC_Start(s_hadc) != HAL_OK) { s_dat.ok = 0; return false; }

        if (HAL_ADC_PollForConversion(s_hadc, RAIN_POLL_TIMEOUT_MS) != HAL_OK) {
            HAL_ADC_Stop(s_hadc);
            s_dat.ok = 0;
            return false;
        }

        sum += HAL_ADC_GetValue(s_hadc);
        HAL_ADC_Stop(s_hadc);
    }

    uint16_t raw = (uint16_t)(sum / RAIN_SAMPLES_PER);
    if (raw > 4095u) raw = 4095u;

    s_dat.raw  = raw;
    s_dat.pct  = _raw_to_pct(raw, s_cfg.raw_dry, s_cfg.raw_wet);
    s_dat.ts_ms = HAL_GetTick();
    s_dat.ok   = 1;

    /* 下雨判定（基于 pct + 回差） */
    if (!s_cfg.en_hysteresis) {
        s_is_raining = (s_dat.pct >= s_cfg.rain_on_pct);
    } else {
        if (!s_is_raining) {
            if (s_dat.pct >= s_cfg.rain_on_pct) s_is_raining = true;
        } else {
            if (s_dat.pct <= s_cfg.rain_off_pct) s_is_raining = false;
        }
    }

    return true;
}

void drv_rain_get(rain_data_t *out)
{
    if (!out) return;
    *out = s_dat;
}

bool drv_rain_is_raining(void)
{
    return s_is_raining;
}
