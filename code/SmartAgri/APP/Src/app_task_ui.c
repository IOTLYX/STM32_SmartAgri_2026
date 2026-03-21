#include "app_task_ui.h"
#include "app_task_ctrl.h"
#include "app_data.h"
#include "app_cfg.h"
#include "app_types.h"

#include "cmsis_os2.h"
#include "bsp_key.h"
#include "svc_ui.h"

#include <string.h>

static app_ui_mode_t app_ui_mode_next(app_ui_mode_t mode)
{
    mode = (app_ui_mode_t)(mode + 1);

    if (mode >= APP_UI_MODE_COUNT)
    {
        mode = APP_UI_MODE_MONITOR;
    }

    return mode;
}

void uiTaskStart(void *argument)
{
    (void)argument;

    uint32_t next_wake = osKernelGetTickCount();
    uint32_t refresh_cnt = 0U;
    uint8_t  key_trg = 0U;
    app_snapshot_t snap;
    app_ui_mode_t ui_mode = APP_UI_MODE_MONITOR;

    memset(&snap, 0, sizeof(snap));

    bsp_key_init();
    svc_ui_init();

    app_data_set_ui_mode(ui_mode);

    for (;;)
    {
        bsp_key_scan_10ms();
        key_trg = bsp_key_trg_get();

        if ((key_trg & KEY_MASK_PREV) != 0U)
        {
            app_ctrl_request_buzz_ack();
        }

        if ((key_trg & KEY_MASK_MODE) != 0U)
        {
            ui_mode = app_ui_mode_next(ui_mode);
            app_data_set_ui_mode(ui_mode);
            svc_ui_force_refresh();
        }

        if ((key_trg & KEY_MASK_NEXT) != 0U)
        {
            svc_ui_force_refresh();
        }

        refresh_cnt += APP_TASK_UI_SCAN_PERIOD_MS;
        if (refresh_cnt >= APP_TASK_UI_REFRESH_MS)
        {
            refresh_cnt = 0U;
            app_data_get_snapshot(&snap);
            svc_ui_render(&snap);
        }

        next_wake += APP_TASK_UI_SCAN_PERIOD_MS;
        (void)osDelayUntil(next_wake);
    }
}

