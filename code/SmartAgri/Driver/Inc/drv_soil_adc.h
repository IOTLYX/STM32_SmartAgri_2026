#ifndef DRV_SOIL_ADC_H
#define DRV_SOIL_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 土壤湿度探头数量
 *
 * 当前工程使用 3 路土壤湿度 ADC 通道：
 * - ADC1_IN1
 * - ADC1_IN2
 * - ADC1_IN3
 */
#define SOIL_CH_NUM  3u

/**
 * @brief 初始化土壤湿度驱动
 *
 * 绑定底层 ADC 句柄，供后续多通道采样使用。
 *
 * @param hadc ADC 句柄，通常为 hadc1
 */
void drv_soil_init(ADC_HandleTypeDef *hadc);

/**
 * @brief 执行一次土壤湿度采样
 *
 * 对每一路土壤探头进行 5 次采样求平均，
 * 再对三路结果求平均，最终换算为土壤湿度百分比。
 *
 * 说明：
 * - 原始 ADC 值范围通常为 0~4095
 * - 百分比范围为 0~100
 * - 数值越大表示土壤越湿
 *
 * @return true  采样成功并更新内部缓存
 * @return false 采样失败
 */
bool drv_soil_sample(void);

/**
 * @brief 获取最终土壤湿度百分比
 *
 * 百分比范围为 0~100，数值越大表示越湿。
 *
 * @return 最近一次计算得到的土壤湿度百分比
 */
uint8_t drv_soil_get_pct(void);

/**
 * @brief 获取最终平均 ADC 原始值
 *
 * 返回三路探头平均后的最终 ADC 原始值。
 * 原始值范围通常为 0~4095，数值越小通常表示越湿。
 *
 * @return 最近一次计算得到的平均 ADC 原始值
 */
uint16_t drv_soil_get_raw(void);

/**
 * @brief 获取每一路探头的平均 ADC 原始值
 *
 * 通过输出数组返回 3 路探头各自的平均 ADC 原始值。
 *
 * @param out_raw3 输出数组，长度应为 @ref SOIL_CH_NUM
 * @return true  获取成功
 * @return false 参数错误或当前数据无效
 */
bool drv_soil_get_raw3(uint16_t out_raw3[SOIL_CH_NUM]);

#ifdef __cplusplus
}
#endif

#endif /* DRV_SOIL_ADC_H */