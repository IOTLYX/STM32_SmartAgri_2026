#include "drv_rain_adc.h"
#include <string.h>

/* ===== 参数 ===== */
#define RAIN_SAMPLES_PER       5   /* 单次测量的采样次数 */
#define RAIN_POLL_TIMEOUT_MS   3   /* ADC 转换等待超时时间，单位 ms */

/* 雨滴传感器所使用的 ADC 句柄 */
static ADC_HandleTypeDef *s_hadc = NULL;

/* 最近一次雨滴采样数据缓存 */
static rain_data_t s_dat = {0};

/* 当前雨滴传感器配置参数 */
static rain_cfg_t s_cfg = {0};

/* 当前下雨状态标志 */
static bool s_is_raining = false;

/**
 * @brief 将整数限制在指定 uint8_t 范围内
 *
 * @param v  输入值
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
 * @brief 将 ADC 原始值换算为湿润百分比
 *
 * 百分比范围为 0~100，数值越大表示越湿/越可能下雨。
 *
 * 兼容两种传感器方向：
 * 1. 常见情况：湿时 ADC 更小（raw_wet < raw_dry）
 * 2. 另一种情况：湿时 ADC 更大（raw_wet > raw_dry）
 *
 * @param raw      当前 ADC 原始值
 * @param raw_dry  干燥标定值
 * @param raw_wet  湿润标定值
 * @return 换算后的湿润百分比
 */
static uint8_t _raw_to_pct(uint16_t raw, uint16_t raw_dry, uint16_t raw_wet)
{
    if (raw_dry == raw_wet)
    {
        return 0;
    }

    int32_t pct;

    if (raw_wet > raw_dry)
    {
        /* 湿时更大：pct = (raw - raw_dry) / (raw_wet - raw_dry) */
        pct = ((int32_t)raw - (int32_t)raw_dry) * 100
            / ((int32_t)raw_wet - (int32_t)raw_dry);
    }
    else
    {
        /* 湿时更小：pct = (raw_dry - raw) / (raw_dry - raw_wet) */
        pct = ((int32_t)raw_dry - (int32_t)raw) * 100
            / ((int32_t)raw_dry - (int32_t)raw_wet);
    }

    return _clamp_u8(pct, 0, 100);
}

/**
 * @brief 将 ADC 规则通道切换到 ADC1_IN9
 *
 * 雨滴传感器固定使用 ADC_CHANNEL_9。
 * 若不在采样前显式切换，可能会受到其他 ADC 通道配置残留影响。
 *
 * @return true  通道切换成功
 * @return false 通道切换失败
 */
static bool _select_channel_in9(void)
{
    if (!s_hadc)
    {
        return false;
    }

    ADC_ChannelConfTypeDef cfg = {0};
    cfg.Channel = ADC_CHANNEL_9;                  /* ADC1_IN9 */
    cfg.Rank = ADC_REGULAR_RANK_1;
    cfg.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;

    return (HAL_ADC_ConfigChannel(s_hadc, &cfg) == HAL_OK);
}

/**
 * @brief 初始化雨滴传感器驱动
 *
 * 保存 ADC 句柄，清空内部缓存，并加载默认标定参数和阈值配置。
 *
 * 默认配置：
 * - raw_dry = 3500
 * - raw_wet = 1500
 * - pct >= 60 认为下雨
 * - pct <= 45 解除下雨状态
 *
 * @param hadc ADC 句柄
 */
void drv_rain_init(ADC_HandleTypeDef *hadc)
{
    s_hadc = hadc;

    /* 清空数据缓存和配置 */
    memset(&s_dat, 0, sizeof(s_dat));
    memset(&s_cfg, 0, sizeof(s_cfg));

    /* 默认标定值，后续建议根据实测结果覆盖 */
    s_cfg.raw_dry = 3500;
    s_cfg.raw_wet = 1500;

    /* 默认下雨判定阈值 */
    s_cfg.rain_on_pct = 60;
    s_cfg.rain_off_pct = 45;
    s_cfg.en_hysteresis = 1;

    s_is_raining = false;
}

/**
 * @brief 设置雨滴传感器配置参数
 *
 * 包括干湿标定点、下雨触发阈值、解除阈值和回差使能。
 * 若开启回差且用户配置了 rain_on_pct < rain_off_pct，
 * 则自动交换两个阈值，避免逻辑错误。
 *
 * @param cfg 配置参数指针
 */
void drv_rain_set_cfg(const rain_cfg_t *cfg)
{
    if (!cfg)
    {
        return;
    }

    s_cfg = *cfg;

    /* 限制阈值范围在 0~100 */
    s_cfg.rain_on_pct = _clamp_u8(s_cfg.rain_on_pct, 0, 100);
    s_cfg.rain_off_pct = _clamp_u8(s_cfg.rain_off_pct, 0, 100);

    if (s_cfg.en_hysteresis && s_cfg.rain_on_pct < s_cfg.rain_off_pct)
    {
        /* 若用户把触发/解除阈值写反，则自动交换 */
        uint8_t t = s_cfg.rain_on_pct;
        s_cfg.rain_on_pct = s_cfg.rain_off_pct;
        s_cfg.rain_off_pct = t;
    }
}

/**
 * @brief 执行一次雨滴传感器采样
 *
 * 采样流程：
 * 1. 切换到 ADC1_IN9 通道
 * 2. 连续采样 5 次并求平均
 * 3. 将平均原始值换算为湿润百分比
 * 4. 更新数据缓存与下雨状态
 *
 * @return true  采样成功
 * @return false 采样失败
 */
bool drv_rain_sample(void)
{
    if (!s_hadc)
    {
        s_dat.ok = 0;
        return false;
    }

    /* 关键：采样前切到 IN9，避免被其他 ADC 通道配置影响 */
    if (!_select_channel_in9())
    {
        s_dat.ok = 0;
        return false;
    }

    uint32_t sum = 0;

    for (int i = 0; i < RAIN_SAMPLES_PER; i++)
    {
        if (HAL_ADC_Start(s_hadc) != HAL_OK)
        {
            s_dat.ok = 0;
            return false;
        }

        if (HAL_ADC_PollForConversion(s_hadc, RAIN_POLL_TIMEOUT_MS) != HAL_OK)
        {
            HAL_ADC_Stop(s_hadc);
            s_dat.ok = 0;
            return false;
        }

        sum += HAL_ADC_GetValue(s_hadc);
        HAL_ADC_Stop(s_hadc);
    }

    /* 计算 5 次平均值 */
    uint16_t raw = (uint16_t)(sum / RAIN_SAMPLES_PER);
    if (raw > 4095u)
    {
        raw = 4095u;
    }

    /* 更新数据缓存 */
    s_dat.raw = raw;
    s_dat.pct = _raw_to_pct(raw, s_cfg.raw_dry, s_cfg.raw_wet);
    s_dat.ts_ms = HAL_GetTick();
    s_dat.ok = 1;

    /* 根据当前湿润百分比和回差配置更新下雨状态 */
    if (!s_cfg.en_hysteresis)
    {
        s_is_raining = (s_dat.pct >= s_cfg.rain_on_pct);
    }
    else
    {
        if (!s_is_raining)
        {
            if (s_dat.pct >= s_cfg.rain_on_pct)
            {
                s_is_raining = true;
            }
        }
        else
        {
            if (s_dat.pct <= s_cfg.rain_off_pct)
            {
                s_is_raining = false;
            }
        }
    }

    return true;
}

/**
 * @brief 获取最近一次雨滴传感器数据
 *
 * @param out 输出数据指针
 */
void drv_rain_get(rain_data_t *out)
{
    if (!out)
    {
        return;
    }

    *out = s_dat;
}

/**
 * @brief 获取当前是否处于下雨状态
 *
 * @return true  当前判定为下雨
 * @return false 当前判定为未下雨
 */
bool drv_rain_is_raining(void)
{
    return s_is_raining;
}