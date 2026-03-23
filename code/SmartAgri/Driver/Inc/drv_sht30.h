#ifndef DRV_SHT30_H
#define DRV_SHT30_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SHT30 温湿度数据缓存
 *
 * 用于保存最近一次采样得到的温度、湿度及状态信息。
 */
typedef struct
{
    int16_t  temp_x10;   /**< 温度值，单位 0.1℃，例如 253 表示 25.3℃ */
    uint16_t hum_x10;    /**< 湿度值，单位 0.1%RH，例如 605 表示 60.5%RH */
    uint32_t ts_ms;      /**< 最近一次更新时间戳，单位 ms（HAL_GetTick） */
    uint8_t  ok;         /**< 最近一次更新是否成功，1=成功，0=失败 */
} sht30_data_t;

/**
 * @brief SHT30 温湿度阈值配置
 *
 * 用于高低温、高低湿告警判断。
 */
typedef struct
{
    int16_t  temp_hi_x10; /**< 高温阈值，单位 0.1℃ */
    int16_t  temp_lo_x10; /**< 低温阈值，单位 0.1℃ */
    uint16_t hum_hi_x10;  /**< 高湿阈值，单位 0.1%RH */
    uint16_t hum_lo_x10;  /**< 低湿阈值，单位 0.1%RH */

    uint8_t  en_temp_hi : 1; /**< 是否启用高温告警 */
    uint8_t  en_temp_lo : 1; /**< 是否启用低温告警 */
    uint8_t  en_hum_hi  : 1; /**< 是否启用高湿告警 */
    uint8_t  en_hum_lo  : 1; /**< 是否启用低湿告警 */
} sht30_th_t;

/**
 * @brief SHT30 告警位定义
 *
 * 采用位标志方式，可组合表示多个告警状态。
 */
typedef enum
{
    SHT30_ALM_NONE    = 0,         /**< 无告警 */
    SHT30_ALM_TEMP_HI = (1u << 0), /**< 高温告警 */
    SHT30_ALM_TEMP_LO = (1u << 1), /**< 低温告警 */
    SHT30_ALM_HUM_HI  = (1u << 2), /**< 高湿告警 */
    SHT30_ALM_HUM_LO  = (1u << 3), /**< 低湿告警 */
    SHT30_ALM_SENSOR  = (1u << 7), /**< 传感器异常告警 */
} sht30_alarm_t;

/**
 * @brief 初始化 SHT30 驱动
 *
 * 绑定 I2C 句柄并设置设备地址。
 *
 * @param hi2c  I2C 句柄
 * @param addr7 设备 7bit 地址，常见为 0x44 或 0x45
 */
void drv_sht30_init(I2C_HandleTypeDef *hi2c, uint8_t addr7);

/**
 * @brief 触发并完成一次 SHT30 采样
 *
 * 该接口设计为非阻塞轮询方式。
 * 返回 true 表示本次调用已经完成读数并更新内部缓存；
 * 返回 false 表示当前仍在等待测量完成或本次采样失败。
 *
 * 典型用法：每隔约 100 ms 周期调用一次。
 *
 * @return true  本次调用成功完成采样并更新缓存
 * @return false 仍在等待测量完成或采样失败
 */
bool drv_sht30_sample(void);

/**
 * @brief 获取最近一次温湿度数据
 *
 * 通过拷贝方式输出内部缓存内容。
 *
 * @param out 输出数据指针
 */
void drv_sht30_get(sht30_data_t *out);

/**
 * @brief 设置温湿度告警阈值
 *
 * @param th 阈值配置指针
 */
void drv_sht30_set_threshold(const sht30_th_t *th);

/**
 * @brief 根据当前缓存和阈值配置计算告警位
 *
 * @return 当前告警位组合值，详见 @ref sht30_alarm_t
 */
uint8_t drv_sht30_alarm_eval(void);

#ifdef __cplusplus
}
#endif

#endif /* DRV_SHT30_H */