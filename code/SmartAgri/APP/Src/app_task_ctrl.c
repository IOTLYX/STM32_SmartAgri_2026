#include "app_task_ctrl.h"
#include "app_data.h"
#include "app_cfg.h"

#include <string.h>
#include "cmsis_os2.h"

#include "bsp_led.h"
#include "bsp_buzz.h"
#include "tim.h"
#include "svc_alarm.h"

void app_ctrl_request_buzz_ack(void)
{
    svc_alarm_ack_toggle();
}

static void app_ctrl_init_once(void)
{
    bsp_led_init();
    bsp_buzz_init(&htim3, TIM_CHANNEL_1);
    svc_alarm_init();
}

void ctrlTaskStart(void *argument)
{
    (void)argument;

    uint32_t next_wake = osKernelGetTickCount();
    uint32_t last_uptime_tick = osKernelGetTickCount();
    uint32_t uptime_s = 0U;
    uint8_t heart_div = 0U;

    app_snapshot_t snap;
    app_alarm_data_t alarm;

    memset(&snap, 0, sizeof(snap));
    memset(&alarm, 0, sizeof(alarm));

    app_ctrl_init_once();

    for (;;)
    {
        bsp_led_step_10ms();
        bsp_buzz_step_10ms();

        app_data_get_snapshot(&snap);
        svc_alarm_process(&snap, &alarm);

        if ((alarm.buzz_muted != 0U) && (alarm.buzz_mode != BUZZ_MODE_OFF))
        {
            bsp_buzz_set_mode(BUZZ_MODE_OFF);
        }
        else
        {
            bsp_buzz_set_mode((buzz_mode_t)alarm.buzz_mode);
        }

        bsp_led_set_net_ok((snap.net.wifi_ok != 0U) ? true : false);
        bsp_led_set_alarm((led_alarm_t)alarm.led_alarm_mode);

        app_data_set_alarm(&alarm);

        if ((osKernelGetTickCount() - last_uptime_tick) >= 1000U)
        {
            last_uptime_tick += 1000U;
            uptime_s++;
            app_data_set_uptime(uptime_s);
        }

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