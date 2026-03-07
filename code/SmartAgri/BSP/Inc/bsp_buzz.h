#ifndef BSP_BUZZ_H
#define BSP_BUZZ_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

typedef enum {
    BUZZ_MODE_OFF = 0,
    BUZZ_MODE_DRY,       // 缺水：高频/低频快速轮流
    BUZZ_MODE_RAIN,      // 降雨：三连响 --- 三连响 ---
    BUZZ_MODE_NET_LOST,  // 断网：滴-----滴-----
} buzz_mode_t;

/**
 * @brief 初始化蜂鸣器（TIM3 PWM CH1 / PA6）
 * @param htim    TIM3 句柄（CubeMX 生成的 &htim3）
 * @param channel TIM_CHANNEL_1
 *
 * 说明：默认会启动 PWM，但占空比=0（无声）
 */
void bsp_buzz_init(TIM_HandleTypeDef *htim, uint32_t channel);

/**
 * @brief 设置蜂鸣器模式（非阻塞）
 */
void bsp_buzz_set_mode(buzz_mode_t mode);

/**
 * @brief 10ms 步进函数：放到 task_10ms() 里每 10ms 调用一次
 */
void bsp_buzz_step_10ms(void);

/**
 * @brief 立即静音（保持 PWM 开着但占空比=0）
 */
void bsp_buzz_off(void);

#endif
