#ifndef DRV_BH1750_H
#define DRV_BH1750_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

typedef struct {
    uint32_t lux;      // lux
    uint32_t ts_ms;    // 更新时间戳（HAL_GetTick）
    uint8_t  ok;       // 1=最近一次更新成功
} bh1750_data_t;

typedef struct {
    uint32_t lux_lo;   // 低光照阈值
    uint32_t lux_hi;   // 高光照阈值
    uint8_t  en_lux_lo:1;
    uint8_t  en_lux_hi:1;
} bh1750_th_t;

typedef enum {
    BH1750_ALM_NONE   = 0,
    BH1750_ALM_LUX_LO = (1u << 0),
    BH1750_ALM_LUX_HI = (1u << 1),
    BH1750_ALM_SENSOR = (1u << 7),
} bh1750_alarm_t;

/**
 * @brief 初始化 BH1750
 * @param hi2c   I2C句柄
 * @param addr7  7bit地址：常见 0x23 或 0x5C
 */
void drv_bh1750_init(I2C_HandleTypeDef *hi2c, uint8_t addr7);

/**
 * @brief 触发/完成一次采样（非阻塞）
 * @return true=本次调用完成了“读数并更新缓存”；false=还在等待转换或失败
 *
 * 用法：每100ms调一次即可
 */
bool drv_bh1750_sample(void);

/**
 * @brief 获取最近一次数据（拷贝输出）
 */
void drv_bh1750_get(bh1750_data_t *out);

/**
 * @brief 设置阈值（可选）
 */
void drv_bh1750_set_threshold(const bh1750_th_t *th);

/**
 * @brief 根据当前缓存+阈值生成告警位
 */
uint8_t drv_bh1750_alarm_eval(void);

#endif
