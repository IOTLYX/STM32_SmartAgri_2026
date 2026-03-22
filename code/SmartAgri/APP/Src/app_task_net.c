#include "app_task_net.h"
#include "app_data.h"
#include "app_cfg.h"

#include <string.h>
#include "cmsis_os2.h"

#include "usart.h"
#include "svc_net.h"
#include "bsp_log.h"

void netTaskStart(void *argument)
{
    (void)argument;

    uint32_t next_wake = osKernelGetTickCount();
    uint32_t last_try_ms = 0U;

    app_snapshot_t snap;
    app_net_data_t net;

    memset(&snap, 0, sizeof(snap));
    memset(&net, 0, sizeof(net));

    bsp_log_init(&huart1);
    svc_net_init(&huart2, &huart1);

    /* netTask 首次运行先延时 3000ms */
    static uint8_t s_net_boot_wait_done = 0U;

    if (s_net_boot_wait_done == 0U)
    {
        s_net_boot_wait_done = 1U;
        osDelay(1000U);
    }

    for (;;)
    {
        if ((net.wifi_ok == 0U) || (net.mqtt_ok == 0U))
        {
            uint32_t now = HAL_GetTick();

            if ((last_try_ms == 0U) ||
                ((uint32_t)(now - last_try_ms) >= APP_NET_RETRY_COOLDOWN_MS))
            {
                last_try_ms = now;
                (void)svc_net_ensure_up(&net);
                app_data_set_net(&net);
            }
        }
        else
        {
            app_data_get_snapshot(&snap);

            if (svc_net_publish_snapshot(&snap, &net) == true)
            {
                net.pub_ok_cnt++;
                net.pub_fail_streak = 0U;
            }
            else
            {
                net.pub_fail_cnt++;
                if (net.pub_fail_streak < 255U)
                {
                    net.pub_fail_streak++;
                }

                if (net.pub_fail_streak >= 2U)
                {
                    net.pub_fail_streak = 0U;
                    net.wifi_ok = 0U;
                    net.mqtt_ok = 0U;
                }
            }

            app_data_set_net(&net);
        }

        next_wake += APP_TASK_NET_PERIOD_MS;
        (void)osDelayUntil(next_wake);
    }
}