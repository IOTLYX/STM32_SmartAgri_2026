#ifndef DRV_BH1750_H
#define DRV_BH1750_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BH1750 光照数据缓存
 *
 * 用于保存最近一次采样得到的光照值及其状态。
 */
typedef struct
{
    uint32_t lux;      /**< 光照强度，单位 lux */
    uint32_t ts_ms;    /**< 最近一次更新时间戳，单位 ms（HAL_GetTick） */
    uint8_t  ok;       /**< 最近一次更新是否成功，1=成功，0=失败 */
} bh1750_data_t;

/**
 * @brief BH1750 光照阈值配置
 *
 * 可用于低光照、高光照告警判断。
 */
typedef struct
{
    uint32_t lux_lo;   /**< 低光照阈值 */
    uint32_t lux_hi;   /**< 高光照阈值 */
    uint8_t  en_lux_lo : 1;   /**< 是否启用低光照阈值判断 */
    uint8_t  en_lux_hi : 1;   /**< 是否启用高光照阈值判断 */
} bh1750_th_t;

/**
 * @brief BH1750 告警位定义
 *
 * 采用位标志方式，可组合使用。
 */
typedef enum
{
    BH1750_ALM_NONE   = 0,         /**< 无告警 */
    BH1750_ALM_LUX_LO = (1u << 0), /**< 低光照告警 */
    BH1750_ALM_LUX_HI = (1u << 1), /**< 高光照告警 */
    BH1750_ALM_SENSOR = (1u << 7), /**< 传感器异常告警 */
} bh1750_alarm_t;

/**
 * @brief 初始化 BH1750 驱动
 *
 * 绑定 I2C 句柄并设置设备地址。
 *
 * @param hi2c  I2C 句柄
 * @param addr7 设备 7bit 地址，常见为 0x23 或 0x5C
 */
void drv_bh1750_init(I2C_HandleTypeDef *hi2c, uint8_t addr7);

/**
 * @brief 触发并完成一次 BH1750 采样
 *
 * 该接口设计为非阻塞轮询方式。
 * 返回 true 表示本次调用已经完成读数并更新内部缓存；
 * 返回 false 表示当前仍在等待转换完成或本次采样失败。
 *
 * 典型用法：每隔约 100 ms 周期调用一次。
 *
 * @return true  本次调用成功完成采样并更新缓存
 * @return false 仍在等待转换完成或本次采样失败
 */
bool drv_bh1750_sample(void);

/**
 * @brief 获取最近一次缓存的光照数据
 *
 * 通过拷贝方式输出内部缓存内容。
 *
 * @param out 输出数据指针
 */
void drv_bh1750_get(bh1750_data_t *out);

/**
 * @brief 设置光照阈值
 *
 * 用于配置低光照/高光照告警判断阈值。
 *
 * @param th 阈值配置指针
 */
void drv_bh1750_set_threshold(const bh1750_th_t *th);

/**
 * @brief 根据当前缓存和阈值配置计算告警位
 *
 * @return 当前告警位组合值，详见 @ref bh1750_alarm_t
 */
uint8_t drv_bh1750_alarm_eval(void);

#ifdef __cplusplus
}
#endif

#endif /* DRV_BH1750_H */