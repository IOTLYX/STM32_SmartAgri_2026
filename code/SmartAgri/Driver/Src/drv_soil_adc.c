#include "drv_soil_adc.h"
#include <string.h>

/* ===== 采样参数 ===== */
#define SOIL_SAMPLES_PER_CH     5u
#define SOIL_POLL_TIMEOUT_MS    10u

/* 土壤模块一般源阻抗偏高，采样时间建议长一点 */
#define SOIL_ADC_SAMPLETIME     ADC_SAMPLETIME_239CYCLES_5

/* 三路固定映射：ADC1_IN1/IN2/IN3 */
static const uint32_t s_adc_ch[SOIL_CH_NUM] = {
    ADC_CHANNEL_1,
    ADC_CHANNEL_4,
    ADC_CHANNEL_5
};

static ADC_HandleTypeDef *s_hadc = NULL;

static uint16_t s_raw3[SOIL_CH_NUM] = {0};  // 每路5次平均后的raw
static uint16_t s_raw_avg = 0;              // 三路再平均
static uint8_t  s_pct = 0;                  // 0~100
static uint8_t  s_ok  = 0;

/* clamp */
static uint8_t _clamp_u8(int32_t v, uint8_t lo, uint8_t hi)
{
    if (v < (int32_t)lo) return lo;
    if (v > (int32_t)hi) return hi;
    return (uint8_t)v;
}

/* 配置通道 */
static bool _cfg_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef c = {0};
    c.Channel      = channel;
    c.Rank         = ADC_REGULAR_RANK_1;
    c.SamplingTime = SOIL_ADC_SAMPLETIME;
    return (HAL_ADC_ConfigChannel(s_hadc, &c) == HAL_OK);
}

/* 读一次 12bit */
static bool _read_once_u12(uint16_t *out_u12)
{
    if (HAL_ADC_Start(s_hadc) != HAL_OK) return false;

    if (HAL_ADC_PollForConversion(s_hadc, SOIL_POLL_TIMEOUT_MS) != HAL_OK) {
        (void)HAL_ADC_Stop(s_hadc);
        return false;
    }

    uint32_t v = HAL_ADC_GetValue(s_hadc);
    (void)HAL_ADC_Stop(s_hadc);

    if (v > 4095u) v = 4095u;
    *out_u12 = (uint16_t)v;
    return true;
}

/* 读某路：切通道dummy一次 + 5次平均 */
static bool _read_avg_5(uint32_t channel, uint16_t *out_avg)
{
    if (!_cfg_channel(channel)) return false;

    /* 切通道后先dummy一次，减少多路复用残留影响 */
    uint16_t dummy = 0;
    (void)_read_once_u12(&dummy);

    uint32_t sum = 0;
    for (uint32_t i = 0; i < SOIL_SAMPLES_PER_CH; i++) {
        uint16_t v = 0;
        if (!_read_once_u12(&v)) return false;
        sum += v;
    }

    *out_avg = (uint16_t)(sum / SOIL_SAMPLES_PER_CH);
    return true;
}

void drv_soil_init(ADC_HandleTypeDef *hadc)
{
    s_hadc = hadc;

    memset(s_raw3, 0, sizeof(s_raw3));
    s_raw_avg = 0;
    s_pct = 0;
    s_ok  = 0;

    /* 可选：做一次ADC校准（STM32F1常用） */
    if (s_hadc) {
        (void)HAL_ADCEx_Calibration_Start(s_hadc);
    }
}

bool drv_soil_sample(void)
{
    if (!s_hadc) {
        s_ok = 0;
        return false;
    }

    uint32_t sum3 = 0;

    for (uint32_t i = 0; i < SOIL_CH_NUM; i++) {
        uint16_t raw = 0;
        if (!_read_avg_5(s_adc_ch[i], &raw)) {
            s_ok = 0;
            return false;
        }
        s_raw3[i] = raw;
        sum3 += raw;
    }

    /* 三路再平均一次 */
    s_raw_avg = (uint16_t)(sum3 / SOIL_CH_NUM);

    /* 你给的规律：越湿数值越小（0~4095）
       用全量程反比映射到0~100：
       raw=4095(最干) -> 0%
       raw=0(最湿)    -> 100% */
    int32_t pct = ((int32_t)(4095 - s_raw_avg) * 100) / 4095;
    s_pct = _clamp_u8(pct, 0, 100);

    s_ok = 1;
    return true;
}

uint8_t drv_soil_get_pct(void)
{
    return s_ok ? s_pct : 0;
}

uint16_t drv_soil_get_raw(void)
{
    return s_ok ? s_raw_avg : 0;
}

bool drv_soil_get_raw3(uint16_t out_raw3[SOIL_CH_NUM])
{
    if (!out_raw3) return false;
    if (!s_ok) {
        out_raw3[0] = out_raw3[1] = out_raw3[2] = 0;
        return false;
    }
    out_raw3[0] = s_raw3[0];
    out_raw3[1] = s_raw3[1];
    out_raw3[2] = s_raw3[2];
    return true;
}
