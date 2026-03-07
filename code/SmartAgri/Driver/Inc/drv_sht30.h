#ifndef DRV_SHT30_H
#define DRV_SHT30_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

typedef struct {
    int16_t  temp_x10;     // 0.1℃（例：253=25.3℃）
    uint16_t hum_x10;      // 0.1%RH（例：605=60.5%）
    uint32_t ts_ms;        // 更新时间戳（HAL_GetTick）
    uint8_t  ok;           // 1=最近一次更新成功
} sht30_data_t;

typedef struct {
    int16_t  temp_hi_x10;
    int16_t  temp_lo_x10;
    uint16_t hum_hi_x10;
    uint16_t hum_lo_x10;

    uint8_t  en_temp_hi:1;
    uint8_t  en_temp_lo:1;
    uint8_t  en_hum_hi:1;
    uint8_t  en_hum_lo:1;
} sht30_th_t;

typedef enum {
    SHT30_ALM_NONE    = 0,
    SHT30_ALM_TEMP_HI = (1u << 0),
    SHT30_ALM_TEMP_LO = (1u << 1),
    SHT30_ALM_HUM_HI  = (1u << 2),
    SHT30_ALM_HUM_LO  = (1u << 3),
    SHT30_ALM_SENSOR  = (1u << 7),
} sht30_alarm_t;

/**
 * @brief 初始化 SHT30
 * @param hi2c   I2C句柄
 * @param addr7  7bit地址：常见 0x44 或 0x45
 */
void drv_sht30_init(I2C_HandleTypeDef *hi2c, uint8_t addr7);

/**
 * @brief 触发/完成一次采样（非阻塞）
 * @return true=本次调用完成读数并更新缓存；false=还在等待或失败
 *
 * 用法：每100ms调一次即可（测量等待仅20ms，基本一次周期内就能读到）
 */
bool drv_sht30_sample(void);

void drv_sht30_get(sht30_data_t *out);

void drv_sht30_set_threshold(const sht30_th_t *th);

uint8_t drv_sht30_alarm_eval(void);

#endif
