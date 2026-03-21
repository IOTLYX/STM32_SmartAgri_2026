#include "app_task_ctrl.h"
#include "app_data.h"
#include "app_cfg.h"

#include <string.h>
#include "cmsis_os2.h"

#include "bsp_led.h"
#include "bsp_buzz.h"
#include "tim.h"

static volatile uint8_t g_buzz_ack_req = 0U;

void app_ctrl_request_buzz_ack(void)
{
    g_buzz_ack_req = 1U;
}

static uint8_t hyst_low_on_high_off(uint8_t st, uint8_t val, uint8_t on_th, uint8_t off_th)
{
    if (st == 0U)
    {
        if (val <= on_th)
        {
            st = 1U;
        }
    }
    else
    {
        if (val >= off_th)
        {
            st = 0U;
        }
    }

    return st;
}

static uint8_t hyst_high_on_low_off(uint8_t st, uint8_t val, uint8_t on_th, uint8_t off_th)
{
    if (st == 0U)
    {
        if (val >= on_th)
        {
            st = 1U;
        }
    }
    else
    {
        if (val <= off_th)
        {
            st = 0U;
        }
    }

    return st;
}

static void app_ctrl_init_once(void)
{
    bsp_led_init();
    bsp_buzz_init(&htim3, TIM_CHANNEL_1);
}

void ctrlTaskStart(void *argument)
{
    (void)argument;

    uint32_t next_wake = osKernelGetTickCount();
    uint32_t last_uptime_tick = osKernelGetTickCount();
    uint32_t uptime_s = 0U;

    uint8_t dry_alarm = 0U;
    uint8_t rain_alarm = 0U;
    uint8_t net_lost_alarm = 0U;

    uint8_t buzz_muted = 0U;
    uint8_t buzz_muted_mode = BUZZ_MODE_OFF;

    uint8_t net_ever_ok = 0U;
    uint32_t net_bad_since = 0U;

    uint8_t heart_div = 0U;

    app_snapshot_t snap;
    app_alarm_data_t alarm;

    memset(&snap, 0, sizeof(snap));
    memset(&alarm, 0, sizeof(alarm));

    app_ctrl_init_once();

    for (;;)
    {
        /* 10ms 节拍执行 */
        bsp_led_step_10ms();
        bsp_buzz_step_10ms();

        app_data_get_snapshot(&snap);

        /* 网络丢失判定：只有“曾经连上过”后再掉线才报警 */
        if (snap.net.wifi_ok != 0U)
        {
            net_ever_ok = 1U;
            net_bad_since = 0U;
            net_lost_alarm = 0U;
        }
        else
        {
            if ((net_ever_ok != 0U) && (net_bad_since == 0U))
            {
                net_bad_since = osKernelGetTickCount();
            }

            if ((net_bad_since != 0U) &&
                ((osKernelGetTickCount() - net_bad_since) >= APP_NET_LOST_ALM_DELAY_MS))
            {
                net_lost_alarm = 1U;
            }
        }

        /* 回差告警 */
        dry_alarm = hyst_low_on_high_off(dry_alarm,
                                         snap.env.soil_pct,
                                         APP_TH_SOIL_DRY_ON_PCT,
                                         APP_TH_SOIL_DRY_OFF_PCT);

        rain_alarm = hyst_high_on_low_off(rain_alarm,
                                          snap.env.rain_pct,
                                          APP_TH_RAIN_ON_PCT,
                                          APP_TH_RAIN_OFF_PCT);

        memset(&alarm, 0, sizeof(alarm));
        alarm.soil_dry = dry_alarm;
        alarm.raining  = rain_alarm;
        alarm.net_lost = net_lost_alarm;
        alarm.alarm_on = (uint8_t)((dry_alarm != 0U) ||
                                   (rain_alarm != 0U) ||
                                   (net_lost_alarm != 0U));

        /* 原始输出模式 */
        if (dry_alarm != 0U)
        {
            alarm.buzz_mode = BUZZ_MODE_DRY;
            alarm.led_alarm_mode = LED_ALM_DRY;
        }
        else if (rain_alarm != 0U)
        {
            alarm.buzz_mode = BUZZ_MODE_RAIN;
            alarm.led_alarm_mode = LED_ALM_RAIN;
        }
        else if (net_lost_alarm != 0U)
        {
            alarm.buzz_mode = BUZZ_MODE_NET_LOST;
            alarm.led_alarm_mode = LED_ALM_NONE;
        }
        else
        {
            alarm.buzz_mode = BUZZ_MODE_OFF;
            alarm.led_alarm_mode = LED_ALM_NONE;
        }

        /* 按键确认静音 */
        if (g_buzz_ack_req != 0U)
        {
            g_buzz_ack_req = 0U;

            if (alarm.buzz_mode == BUZZ_MODE_OFF)
            {
                buzz_muted = 0U;
                buzz_muted_mode = BUZZ_MODE_OFF;
            }
            else if (buzz_muted == 0U)
            {
                buzz_muted = 1U;
                buzz_muted_mode = alarm.buzz_mode;
            }
            else
            {
                buzz_muted = 0U;
                buzz_muted_mode = BUZZ_MODE_OFF;
            }
        }

        /* 告警消失后自动取消静音记忆 */
        if (alarm.buzz_mode == BUZZ_MODE_OFF)
        {
            buzz_muted = 0U;
            buzz_muted_mode = BUZZ_MODE_OFF;
        }

        /* 告警类型变化时，取消旧静音 */
        if ((buzz_muted != 0U) && (alarm.buzz_mode != buzz_muted_mode))
        {
            buzz_muted = 0U;
            buzz_muted_mode = BUZZ_MODE_OFF;
        }

        if ((buzz_muted != 0U) && (alarm.buzz_mode == buzz_muted_mode))
        {
            bsp_buzz_set_mode(BUZZ_MODE_OFF);
        }
        else
        {
            bsp_buzz_set_mode((buzz_mode_t)alarm.buzz_mode);
        }

        alarm.buzz_muted = buzz_muted;

        /* LED 输出 */
        bsp_led_set_net_ok((snap.net.wifi_ok != 0U) ? true : false);
        bsp_led_set_alarm((led_alarm_t)alarm.led_alarm_mode);

        app_data_set_alarm(&alarm);

        /* 1s 运行时间 */
        if ((osKernelGetTickCount() - last_uptime_tick) >= 1000U)
        {
            last_uptime_tick += 1000U;
            uptime_s++;
            app_data_set_uptime(uptime_s);
        }

        /* 心跳灯：500ms 翻转一次 */
        heart_div++;
        if (heart_div >= 50U)
        {
            heart_div = 0U;
            bsp_led_toggle(LED_HEART);
        }

        next_wake += APP_TASK_CTRL_PERIOD_MS;
        (void)osDelayUntil(next_wake);
    }
}