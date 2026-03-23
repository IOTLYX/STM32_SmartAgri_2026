#ifndef BSP_LED_H
#define BSP_LED_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/**
 * @brief LED 编号定义
 */
typedef enum
{
    LED_HEART = 0, /* 心跳灯 */
    LED_NET,       /* 网络状态灯 */
    LED_ALM,       /* 告警状态灯 */
    LED_COUNT
} led_id_t;

/**
 * @brief 告警灯模式定义
 */
typedef enum
{
    LED_ALM_NONE = 0, /* 无告警 */
    LED_ALM_DRY,      /* 缺水告警 */
    LED_ALM_RAIN,     /* 降雨告警 */
} led_alarm_t;

/**
 * @brief 初始化 LED
 *
 * 初始化 PC13 / PC14 / PC15 对应的 LED 控制状态。
 */
void bsp_led_init(void);

/**
 * @brief 点亮指定 LED
 *
 * @param id LED 编号
 */
void bsp_led_on(led_id_t id);

/**
 * @brief 熄灭指定 LED
 *
 * @param id LED 编号
 */
void bsp_led_off(led_id_t id);

/**
 * @brief 翻转指定 LED 状态
 *
 * @param id LED 编号
 */
void bsp_led_toggle(led_id_t id);

/**
 * @brief 设置网络指示灯状态
 *
 * 网络正常时常亮，网络断开时按内部节奏闪烁。
 *
 * @param ok true 表示网络正常，false 表示网络异常
 */
void bsp_led_set_net_ok(bool ok);

/**
 * @brief 设置告警指示灯模式
 *
 * 根据当前告警类型设置告警灯的显示节奏，
 * 一般与蜂鸣器告警模式相对应。
 *
 * @param a 告警灯模式
 */
void bsp_led_set_alarm(led_alarm_t a);

/**
 * @brief LED 10ms 步进处理函数
 *
 * 需要放在固定 10ms 周期任务中调用，
 * 用于推进 LED 内部闪烁状态机。
 */
void bsp_led_step_10ms(void);

#endif /* BSP_LED_H */