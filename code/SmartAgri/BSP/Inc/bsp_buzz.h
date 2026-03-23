#ifndef BSP_BUZZ_H
#define BSP_BUZZ_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/**
 * @brief 蜂鸣器工作模式
 */
typedef enum
{
    BUZZ_MODE_OFF = 0,   /* 关闭蜂鸣器 */
    BUZZ_MODE_DRY,       /* 缺水：高频/低频快速轮流 */
    BUZZ_MODE_RAIN,      /* 降雨：三连响，周期重复 */
    BUZZ_MODE_NET_LOST,  /* 断网：滴-----滴----- */
} buzz_mode_t;

/**
 * @brief 初始化蜂鸣器
 *
 * 使用 TIM3 PWM CH1（PA6）输出驱动蜂鸣器。
 * 初始化完成后默认启动 PWM，但占空比为 0，即默认静音。
 *
 * @param htim    定时器句柄，通常传入 CubeMX 生成的 &htim3
 * @param channel PWM 通道，通常为 TIM_CHANNEL_1
 */
void bsp_buzz_init(TIM_HandleTypeDef *htim, uint32_t channel);

/**
 * @brief 设置蜂鸣器模式
 *
 * 该接口只更新蜂鸣器状态机目标模式，
 * 实际发声节奏由周期调用的步进函数非阻塞执行。
 *
 * @param mode 蜂鸣器目标模式
 */
void bsp_buzz_set_mode(buzz_mode_t mode);

/**
 * @brief 蜂鸣器 10ms 步进处理函数
 *
 * 需要放在固定 10ms 周期任务中调用，
 * 用于推进蜂鸣器内部非阻塞状态机。
 */
void bsp_buzz_step_10ms(void);

/**
 * @brief 立即关闭蜂鸣器声音
 *
 * 保持 PWM 外设运行，但将占空比设置为 0，
 * 从而实现快速静音。
 */
void bsp_buzz_off(void);

#endif /* BSP_BUZZ_H */