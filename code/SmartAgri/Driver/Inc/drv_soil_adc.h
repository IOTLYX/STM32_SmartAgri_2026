#ifndef DRV_SOIL_ADC_H
#define DRV_SOIL_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

    /* 3个土壤探头：ADC1_IN1/IN2/IN3 */
#define SOIL_CH_NUM  3u

    /* 初始化：传入 hadc1 */
    void drv_soil_init(ADC_HandleTypeDef *hadc);

    /* 采样：每路5次平均 -> 三路再平均 -> 计算 soil_pct(0~100) */
    bool drv_soil_sample(void);

    /* 获取最终的土壤湿度百分比（0~100，越大越湿） */
    uint8_t drv_soil_get_pct(void);

    /* 可选：获取最终平均raw（0~4095，越小越湿） */
    uint16_t drv_soil_get_raw(void);

    /* 可选：获取每一路平均raw（0~4095） */
    bool drv_soil_get_raw3(uint16_t out_raw3[SOIL_CH_NUM]);

#ifdef __cplusplus
}
#endif

#endif
