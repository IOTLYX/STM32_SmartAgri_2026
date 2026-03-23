#include "bsp_led.h"

/* ====== LED 电平有效配置 ======
 * 如果外接 LED 为“高电平点亮”，则改为 0
 * 常见 STM32 板载 PC13 LED 多为低电平点亮，但你这里 PC13/14/15 是否低有效，
 * 仍以你的硬件实际连接为准。
 */
#ifndef BSP_LED_ACTIVE_LOW
#define BSP_LED_ACTIVE_LOW  0
#endif

/* ====== 告警灯统一闪烁节奏配置 ======
 * 例如：250ms 翻转一次
 * 则表现为：亮 250ms -> 灭 250ms -> 亮 250ms -> 灭 250ms
 */
#define ALM_BLINK_HALF_MS      250u
#define STEP_MS                10u
#define ALM_HALF_TICKS         (ALM_BLINK_HALF_MS / STEP_MS)   /* 以 10ms 为步进单位 */

/**
 * @brief LED 硬件映射表项
 */
typedef struct
{
    GPIO_TypeDef *port;   /* GPIO 端口 */
    uint16_t pin;         /* GPIO 引脚 */
} led_hw_t;

/* LED 硬件资源映射 */
static const led_hw_t s_led_hw[LED_COUNT] =
{
    [LED_HEART] = {GPIOC, GPIO_PIN_13},
    [LED_NET]   = {GPIOC, GPIO_PIN_14},
    [LED_ALM]   = {GPIOC, GPIO_PIN_15},
};

/* 逻辑状态变量 */
static bool s_net_ok = true;                 /* 当前网络状态 */
static led_alarm_t s_alarm = LED_ALM_NONE;  /* 当前告警模式 */

/* 步进计时变量，单位 10ms */
static uint16_t s_net_tick = 0;             /* 网络灯闪烁计时 */
static uint16_t s_alm_tick = 0;             /* 告警灯闪烁计时 */
static bool     s_alm_level = false;        /* 告警灯当前亮灭状态 */

/**
 * @brief 写入指定 LED 的硬件电平
 *
 * 根据 BSP_LED_ACTIVE_LOW 配置，将逻辑亮灭状态转换成实际 GPIO 电平。
 *
 * @param id LED 编号
 * @param on true-点亮，false-熄灭
 */
static void _hw_write(led_id_t id, bool on)
{
    GPIO_PinState ps;

#if BSP_LED_ACTIVE_LOW
    ps = on ? GPIO_PIN_RESET : GPIO_PIN_SET;
#else
    ps = on ? GPIO_PIN_SET : GPIO_PIN_RESET;
#endif

    HAL_GPIO_WritePin(s_led_hw[id].port, s_led_hw[id].pin, ps);
}

/**
 * @brief 初始化 LED 驱动
 *
 * 默认关闭全部 LED，并设置：
 * - 网络灯默认常亮
 * - 告警灯默认关闭
 * - 心跳灯默认关闭
 *
 * 前提是 CubeMX 已将 PC13 / PC14 / PC15 配置为 GPIO 输出。
 */
void bsp_led_init(void)
{
    /* 初始化时先关闭所有 LED */
    for (int i = 0; i < LED_COUNT; i++)
    {
        _hw_write((led_id_t)i, false);
    }

    /* 初始化逻辑状态 */
    s_net_ok = true;
    s_alarm = LED_ALM_NONE;
    s_net_tick = 0;
    s_alm_tick = 0;
    s_alm_level = false;

    /* 网络默认正常，网络灯常亮 */
    _hw_write(LED_NET, true);
}

/**
 * @brief 点亮指定 LED
 *
 * @param id LED 编号
 */
void bsp_led_on(led_id_t id)
{
    _hw_write(id, true);
}

/**
 * @brief 熄灭指定 LED
 *
 * @param id LED 编号
 */
void bsp_led_off(led_id_t id)
{
    _hw_write(id, false);
}

/**
 * @brief 翻转指定 LED 当前状态
 *
 * @param id LED 编号
 */
void bsp_led_toggle(led_id_t id)
{
    GPIO_PinState cur = HAL_GPIO_ReadPin(s_led_hw[id].port, s_led_hw[id].pin);

#if BSP_LED_ACTIVE_LOW
    bool is_on = (cur == GPIO_PIN_RESET);
#else
    bool is_on = (cur == GPIO_PIN_SET);
#endif

    _hw_write(id, !is_on);
}

/**
 * @brief 设置网络状态指示灯模式
 *
 * - 网络正常：常亮
 * - 网络异常：进入闪烁模式
 *
 * @param ok true-网络正常，false-网络异常
 */
void bsp_led_set_net_ok(bool ok)
{
    s_net_ok = ok;
    s_net_tick = 0;

    if (ok)
    {
        /* 网络正常时常亮 */
        _hw_write(LED_NET, true);
    }
    else
    {
        /* 网络异常时先灭灯，后续由步进函数驱动闪烁 */
        _hw_write(LED_NET, false);
    }
}

/**
 * @brief 设置告警灯模式
 *
 * - 无告警：熄灭
 * - 有告警：从“亮”开始按统一节奏闪烁
 *
 * @param a 告警灯模式
 */
void bsp_led_set_alarm(led_alarm_t a)
{
    s_alarm = a;
    s_alm_tick = 0;

    if (a == LED_ALM_NONE)
    {
        s_alm_level = false;
        _hw_write(LED_ALM, false);
    }
    else
    {
        /* 统一策略：任意告警都从“亮”开始闪 */
        s_alm_level = true;
        _hw_write(LED_ALM, true);
    }
}

/**
 * @brief 告警灯 10ms 步进处理
 *
 * 无告警时保持熄灭；
 * 有告警时按固定频率闪烁。
 */
static void _alarm_step_10ms(void)
{
    if (s_alarm == LED_ALM_NONE)
    {
        _hw_write(LED_ALM, false);
        return;
    }

    if (ALM_HALF_TICKS == 0)
    {
        /* 防御性处理：若配置异常导致分频为 0，则直接常亮 */
        _hw_write(LED_ALM, true);
        return;
    }

    s_alm_tick++;
    if (s_alm_tick >= ALM_HALF_TICKS)
    {
        s_alm_tick = 0;
        s_alm_level = !s_alm_level;
        _hw_write(LED_ALM, s_alm_level);
    }
}

/**
 * @brief 网络灯 10ms 步进处理
 *
 * 网络正常时保持常亮；
 * 网络异常时按固定节奏闪烁。
 */
static void _net_step_10ms(void)
{
    if (s_net_ok)
    {
        _hw_write(LED_NET, true);
        return;
    }

    /* 断网闪烁：250ms 翻转一次 */
    s_net_tick += STEP_MS;
    if (s_net_tick >= 250u)
    {
        s_net_tick = 0;
        bsp_led_toggle(LED_NET);
    }
}

/**
 * @brief LED 10ms 步进函数
 *
 * 需要放在固定 10ms 周期中调用，
 * 用于推进网络灯与告警灯的非阻塞闪烁状态机。
 */
void bsp_led_step_10ms(void)
{
    _net_step_10ms();
    _alarm_step_10ms();
}