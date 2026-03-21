#include "app_task_net.h"
#include "app_data.h"
#include "app_cfg.h"

#include <string.h>
#include "cmsis_os2.h"

void netTaskStart(void *argument)
{
    (void)argument;

    uint32_t next_wake = osKernelGetTickCount();
    app_net_data_t net;

    memset(&net, 0, sizeof(net));

    for (;;)
    {
        /* 第一阶段先不接 ESP8266，只保留任务骨架 */
        app_data_set_net(&net);

        next_wake += APP_TASK_NET_PERIOD_MS;
        (void)osDelayUntil(next_wake);
    }
}
