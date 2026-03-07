#ifndef BSP_LED_H
#define BSP_LED_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

typedef enum {
    LED_HEART = 0,
    LED_NET,
    LED_ALM,
    LED_COUNT
} led_id_t;

typedef enum {
    LED_ALM_NONE = 0,
    LED_ALM_DRY,
    LED_ALM_RAIN,
} led_alarm_t;

/**
 * @brief 初始化 PC13/PC14/PC15
 */
void bsp_led_init(void);

/* 基础控制 */
void bsp_led_on(led_id_t id);
void bsp_led_off(led_id_t id);
void bsp_led_toggle(led_id_t id);

/**
 * @brief 网络指示：ok=常亮；断网=闪烁
 */
void bsp_led_set_net_ok(bool ok);

/**
 * @brief 告警指示：无/缺水/下雨（节奏与蜂鸣器对应）
 */
void bsp_led_set_alarm(led_alarm_t a);

/**
 * @brief 10ms 步进（放到 task_10ms() 每 10ms 调一次）
 */
void bsp_led_step_10ms(void);

#endif
