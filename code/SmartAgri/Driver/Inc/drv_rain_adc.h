#ifndef DRV_RAIN_ADC_H
#define DRV_RAIN_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 雨滴传感器数据缓存
 *
 * 用于保存最近一次 ADC 采样转换后的原始值、雨滴湿润百分比
 * 以及数据更新时间和有效标志。
 */
typedef struct
{
    uint16_t raw;        /**< ADC 原始值（5 次平均） */
    uint8_t  pct;        /**< 0~100，雨滴湿润百分比，数值越大表示越湿/越可能下雨 */
    uint32_t ts_ms;      /**< 最近一次更新时间戳，单位 ms（HAL_GetTick） */
    uint8_t  ok;         /**< 最近一次更新是否有效，1=有效，0=无效 */
} rain_data_t;

/**
 * @brief 雨滴传感器配置参数
 *
 * 包括干湿标定点以及下雨判定阈值配置。
 */
typedef struct
{
    /**
     * @brief 干燥标定 ADC 值
     *
     * 表示传感器在无水滴、较干燥状态下的 ADC 值。
     * 该值建议实测标定，以提高百分比换算准确性。
     */
    uint16_t raw_dry;

    /**
     * @brief 湿润标定 ADC 值
     *
     * 表示传感器在有明显水滴、较湿润状态下的 ADC 值。
     * 该值建议实测标定。
     */
    uint16_t raw_wet;

    /**
     * @brief 下雨触发阈值，单位 %
     *
     * 当湿润百分比大于等于该值时，判定为“正在下雨”。
     */
    uint8_t rain_on_pct;

    /**
     * @brief 下雨解除阈值，单位 %
     *
     * 当湿润百分比小于等于该值时，判定为“未下雨”。
     */
    uint8_t rain_off_pct;

    /**
     * @brief 是否启用回差判断
     *
     * 1：启用回差，避免阈值附近状态抖动
     * 0：不启用回差
     */
    uint8_t en_hysteresis : 1;
} rain_cfg_t;

/**
 * @brief 初始化雨滴传感器驱动
 *
 * 绑定底层 ADC 句柄，供后续采样使用。
 *
 * @param hadc ADC 句柄
 */
void drv_rain_init(ADC_HandleTypeDef *hadc);

/**
 * @brief 设置雨滴传感器配置参数
 *
 * 包括 ADC 标定点、下雨判定阈值及回差控制。
 *
 * @param cfg 配置参数指针
 */
void drv_rain_set_cfg(const rain_cfg_t *cfg);

/**
 * @brief 执行一次雨滴传感器采样
 *
 * 对指定 ADC 通道执行 5 次采样求平均，并更新内部缓存的
 * 原始值和湿润百分比。
 *
 * @return true  采样成功并更新缓存
 * @return false 采样失败
 */
bool drv_rain_sample(void);

/**
 * @brief 获取最近一次雨滴传感器数据
 *
 * 通过拷贝方式输出内部缓存内容。
 *
 * @param out 输出数据指针
 */
void drv_rain_get(rain_data_t *out);

/**
 * @brief 获取当前是否处于下雨状态
 *
 * 基于当前湿润百分比以及配置的阈值/回差进行判定。
 *
 * @return true  当前判定为下雨
 * @return false 当前判定为未下雨
 */
bool drv_rain_is_raining(void);

#ifdef __cplusplus
}
#endif

#endif /* DRV_RAIN_ADC_H */