/**
 * @file drv_soil_adc.c
 * @brief 土壤湿度 ADC 采样驱动实现
 */

#include "drv_soil_adc.h"
#include <string.h>

/* ==================== 采样参数 ==================== */

/** @brief 每个通道的平均采样次数 */
#define SOIL_SAMPLES_PER_CH     5u

/** @brief ADC 轮询转换超时时间，单位 ms */
#define SOIL_POLL_TIMEOUT_MS    10u

/** @brief ADC 采样时间配置（适合高源阻抗土壤模块） */
#define SOIL_ADC_SAMPLETIME     ADC_SAMPLETIME_239CYCLES_5

/* ==================== 通道映射 ==================== */

/**
 * @brief 三路土壤传感器对应的 ADC 通道
 * @note 固定映射为 ADC1_IN1 / IN4 / IN5
 */
static const uint32_t s_adc_ch[SOIL_CH_NUM] =
{
    ADC_CHANNEL_1,
    ADC_CHANNEL_4,
    ADC_CHANNEL_5
};

/** @brief ADC 句柄 */
static ADC_HandleTypeDef *s_hadc = NULL;

/** @brief 三路通道平均后的原始 ADC 值 */
static uint16_t s_raw3[SOIL_CH_NUM] = {0};

/** @brief 三路再次平均后的总原始值 */
static uint16_t s_raw_avg = 0;

/** @brief 当前土壤湿度百分比，范围 0~100 */
static uint8_t s_pct = 0;

/** @brief 当前数据有效标志 */
static uint8_t s_ok = 0;

/**
 * @brief 将整数限制在指定 uint8_t 范围内
 * @param v 输入值
 * @param lo 下限
 * @param hi 上限
 * @return 限幅后的结果
 */
static uint8_t _clamp_u8(int32_t v, uint8_t lo, uint8_t hi)
{
    if (v < (int32_t)lo)
    {
        return lo;
    }

    if (v > (int32_t)hi)
    {
        return hi;
    }

    return (uint8_t)v;
}

/**
 * @brief 配置 ADC 当前规则通道
 * @param channel ADC 通道号
 * @return true 配置成功
 * @return false 配置失败
 */
static bool _cfg_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef c = {0};

    c.Channel      = channel;
    c.Rank         = ADC_REGULAR_RANK_1;
    c.SamplingTime = SOIL_ADC_SAMPLETIME;

    return (HAL_ADC_ConfigChannel(s_hadc, &c) == HAL_OK);
}

/**
 * @brief 读取一次 ADC 12bit 原始值
 * @param out_u12 输出的 ADC 原始值指针
 * @return true 读取成功
 * @return false 读取失败
 */
static bool _read_once_u12(uint16_t *out_u12)
{
    if (HAL_ADC_Start(s_hadc) != HAL_OK)
    {
        return false;
    }

    if (HAL_ADC_PollForConversion(s_hadc, SOIL_POLL_TIMEOUT_MS) != HAL_OK)
    {
        (void)HAL_ADC_Stop(s_hadc);
        return false;
    }

    uint32_t v = HAL_ADC_GetValue(s_hadc);
    (void)HAL_ADC_Stop(s_hadc);

    /* 防御性保护，确保结果落在 12bit 范围内 */
    if (v > 4095u)
    {
        v = 4095u;
    }

    *out_u12 = (uint16_t)v;
    return true;
}

/**
 * @brief 读取指定通道并做 5 次平均
 * @param channel ADC 通道号
 * @param out_avg 输出平均值
 * @return true 采样成功
 * @return false 采样失败
 * @note 切换通道后先做一次 dummy 读取，以减小多路复用残留影响
 */
static bool _read_avg_5(uint32_t channel, uint16_t *out_avg)
{
    if (!_cfg_channel(channel))
    {
        return false;
    }

    /* 切换通道后丢弃第一次结果，降低通道串扰影响 */
    uint16_t dummy = 0;
    (void)_read_once_u12(&dummy);

    uint32_t sum = 0;
    for (uint32_t i = 0; i < SOIL_SAMPLES_PER_CH; i++)
    {
        uint16_t v = 0;
        if (!_read_once_u12(&v))
        {
            return false;
        }
        sum += v;
    }

    *out_avg = (uint16_t)(sum / SOIL_SAMPLES_PER_CH);
    return true;
}

/**
 * @brief 初始化土壤湿度 ADC 驱动
 * @param hadc ADC 句柄
 * @return 无
 */
void drv_soil_init(ADC_HandleTypeDef *hadc)
{
    s_hadc = hadc;

    memset(s_raw3, 0, sizeof(s_raw3));
    s_raw_avg = 0;
    s_pct = 0;
    s_ok  = 0;

    /* STM32F1 常见做法：初始化时执行一次 ADC 校准 */
    if (s_hadc)
    {
        (void)HAL_ADCEx_Calibration_Start(s_hadc);
    }
}

/**
 * @brief 执行一次三路土壤湿度采样
 * @param 无
 * @return true 采样成功
 * @return false 采样失败
 * @note 采样流程：
 *       1. 依次读取三路 ADC
 *       2. 每路做 5 次平均
 *       3. 三路结果再次平均
 *       4. 按反比关系换算为湿度百分比
 */
bool drv_soil_sample(void)
{
    if (!s_hadc)
    {
        s_ok = 0;
        return false;
    }

    uint32_t sum3 = 0;

    for (uint32_t i = 0; i < SOIL_CH_NUM; i++)
    {
        uint16_t raw = 0;
        if (!_read_avg_5(s_adc_ch[i], &raw))
        {
            s_ok = 0;
            return false;
        }

        s_raw3[i] = raw;
        sum3 += raw;
    }

    /* 三路原始值再做一次平均 */
    s_raw_avg = (uint16_t)(sum3 / SOIL_CH_NUM);

    /* 湿度映射规则：
     * raw = 4095 -> 0%   （最干）
     * raw = 0    -> 100% （最湿）
     */
    int32_t pct = ((int32_t)(4095 - s_raw_avg) * 100) / 4095;
    s_pct = _clamp_u8(pct, 0, 100);

    s_ok = 1;
    return true;
}

/**
 * @brief 获取当前土壤湿度百分比
 * @param 无
 * @return 湿度百分比，范围 0~100
 * @note 当前数据无效时返回 0
 */
uint8_t drv_soil_get_pct(void)
{
    return s_ok ? s_pct : 0;
}

/**
 * @brief 获取三路平均后的 ADC 原始值
 * @param 无
 * @return 平均原始值
 * @note 当前数据无效时返回 0
 */
uint16_t drv_soil_get_raw(void)
{
    return s_ok ? s_raw_avg : 0;
}

/**
 * @brief 获取三路通道各自的原始 ADC 值
 * @param out_raw3 输出数组，长度为 SOIL_CH_NUM
 * @return true 获取成功
 * @return false 获取失败或当前数据无效
 */
bool drv_soil_get_raw3(uint16_t out_raw3[SOIL_CH_NUM])
{
    if (!out_raw3)
    {
        return false;
    }

    if (!s_ok)
    {
        out_raw3[0] = 0;
        out_raw3[1] = 0;
        out_raw3[2] = 0;
        return false;
    }

    out_raw3[0] = s_raw3[0];
    out_raw3[1] = s_raw3[1];
    out_raw3[2] = s_raw3[2];

    return true;
}