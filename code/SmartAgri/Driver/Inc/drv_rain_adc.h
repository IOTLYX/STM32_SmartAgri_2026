#ifndef DRV_RAIN_ADC_H
#define DRV_RAIN_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

typedef struct {
    uint16_t raw;        // ADC原始值（5次平均）
    uint8_t  pct;        // 0~100（雨滴湿润百分比：越大越“湿/有雨”）
    uint32_t ts_ms;      // 更新时间戳
    uint8_t  ok;         // 1=有效
} rain_data_t;

typedef struct {
    /* 标定点：干/湿对应ADC（必须你自己测一次更准） */
    uint16_t raw_dry;    // 干燥（无水滴）时ADC
    uint16_t raw_wet;    // 湿润（有水滴）时ADC

    /* 下雨判定阈值（基于 pct） */
    uint8_t  rain_on_pct;      // >= 触发“下雨”
    uint8_t  rain_off_pct;     // <= 解除“下雨”
    uint8_t  en_hysteresis:1;  // 1=带回差
} rain_cfg_t;

void drv_rain_init(ADC_HandleTypeDef *hadc);
void drv_rain_set_cfg(const rain_cfg_t *cfg);

/* 采样一次：ADC1_IN9 采5次平均 -> 更新 raw/pct */
bool drv_rain_sample(void);

void drv_rain_get(rain_data_t *out);

/* 当前是否下雨（基于 pct + 回差） */
bool drv_rain_is_raining(void);

#endif
