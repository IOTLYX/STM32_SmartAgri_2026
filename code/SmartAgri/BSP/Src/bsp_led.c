#include "bsp_led.h"

/* ====== 如果你外接LED是“高电平亮”，改为 0 ======
 * STM32F103C8T6 常见板载PC13 LED是低电平亮，所以默认1
 */
#ifndef BSP_LED_ACTIVE_LOW
#define BSP_LED_ACTIVE_LOW  0
#endif

/* ====== 统一告警灯闪烁频率（ALM）======
 * 例如：250ms 翻转一次 => 亮250ms 灭250ms => 2Hz 闪烁
 * 想更慢：改成 500；想更快：改成 150 等
 */
#define ALM_BLINK_HALF_MS      250u
#define STEP_MS                10u
#define ALM_HALF_TICKS         (ALM_BLINK_HALF_MS / STEP_MS)   /* 10ms为单位 */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} led_hw_t;

static const led_hw_t s_led_hw[LED_COUNT] = {
    [LED_HEART] = {GPIOC, GPIO_PIN_13},
    [LED_NET]   = {GPIOC, GPIO_PIN_14},
    [LED_ALM]   = {GPIOC, GPIO_PIN_15},
};

/* 逻辑层状态 */
static bool s_net_ok = true;
static led_alarm_t s_alarm = LED_ALM_NONE;

/* 步进计时（10ms单位） */
static uint16_t s_net_tick = 0;
static uint16_t s_alm_tick = 0;
static bool     s_alm_level = false;   /* ALM当前亮灭状态 */

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

void bsp_led_init(void)
{
    /* 假设你 CubeMX 已经把 PC13/14/15 配成 GPIO_Output */
    for (int i = 0; i < LED_COUNT; i++) {
        _hw_write((led_id_t)i, false);
    }

    s_net_ok = true;
    s_alarm = LED_ALM_NONE;
    s_net_tick = 0;
    s_alm_tick = 0;
    s_alm_level = false;

    /* 网络默认常亮 */
    _hw_write(LED_NET, true);
}

void bsp_led_on(led_id_t id)     { _hw_write(id, true); }
void bsp_led_off(led_id_t id)    { _hw_write(id, false); }
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

void bsp_led_set_net_ok(bool ok)
{
    s_net_ok = ok;
    s_net_tick = 0;

    if (ok) {
        _hw_write(LED_NET, true);     // 常亮
    } else {
        _hw_write(LED_NET, false);    // 先灭，再由step闪烁
    }
}

void bsp_led_set_alarm(led_alarm_t a)
{
    s_alarm = a;
    s_alm_tick = 0;

    if (a == LED_ALM_NONE) {
        s_alm_level = false;
        _hw_write(LED_ALM, false);
    } else {
        /* ★统一：任何告警都从“亮”开始闪 */
        s_alm_level = true;
        _hw_write(LED_ALM, true);
    }
}

/* ===== 告警灯节奏：统一固定频率闪烁 ===== */
static void _alarm_step_10ms(void)
{
    if (s_alarm == LED_ALM_NONE) {
        _hw_write(LED_ALM, false);
        return;
    }

    /* 10ms步进计数 */
    if (ALM_HALF_TICKS == 0) {
        /* 防御：如果配置太小导致除法为0，直接常亮 */
        _hw_write(LED_ALM, true);
        return;
    }

    s_alm_tick++;
    if (s_alm_tick >= ALM_HALF_TICKS) {
        s_alm_tick = 0;
        s_alm_level = !s_alm_level;
        _hw_write(LED_ALM, s_alm_level);
    }
}

static void _net_step_10ms(void)
{
    if (s_net_ok) {
        _hw_write(LED_NET, true);
        return;
    }

    /* 断网闪烁：250ms 翻转一次（更明显） */
    s_net_tick += 10;
    if (s_net_tick >= 250) {
        s_net_tick = 0;
        bsp_led_toggle(LED_NET);
    }
}

void bsp_led_step_10ms(void)
{
    _net_step_10ms();
    _alarm_step_10ms();
}
