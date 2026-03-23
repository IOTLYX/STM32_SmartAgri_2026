#include "app_task_ui.h"
#include "app_task_ctrl.h"
#include "app_data.h"
#include "app_cfg.h"
#include "app_types.h"

#include "cmsis_os2.h"
#include "bsp_key.h"
#include "svc_ui.h"

#include <string.h>

/**
 * @brief 获取下一个 UI 显示模式
 *
 * 按顺序切换到下一个界面模式，当达到末尾时回绕到默认监控界面。
 *
 * @param mode 当前 UI 模式
 * @return app_ui_mode_t 切换后的 UI 模式
 */
static app_ui_mode_t app_ui_mode_next(app_ui_mode_t mode)
{
    /* 枚举值顺序递增，切换到下一个页面 */
    mode = (app_ui_mode_t)(mode + 1);

    /* 超出范围后回到默认监控页 */
    if (mode >= APP_UI_MODE_COUNT)
    {
        mode = APP_UI_MODE_MONITOR;
    }

    return mode;
}

/**
 * @brief UI 任务入口函数
 *
 * 该任务负责按键扫描、界面模式切换和显示刷新，主要流程如下：
 * 1. 初始化按键驱动与 UI 服务
 * 2. 周期性扫描按键触发事件
 * 3. 处理模式切换、确认提示音、手动刷新请求
 * 4. 按设定周期读取全局快照并渲染界面
 *
 * @param argument 任务入口参数，当前未使用
 */
void uiTaskStart(void *argument)
{
    (void)argument;

    /* 记录下次任务唤醒时刻，用于固定周期调度 */
    uint32_t next_wake = osKernelGetTickCount();

    /* 刷新累计计数，用于实现“扫描周期”和“显示刷新周期”分离 */
    uint32_t refresh_cnt = 0U;

    /* 按键触发标志 */
    uint8_t  key_trg = 0U;

    /* 系统快照缓存 */
    app_snapshot_t snap;

    /* 当前 UI 模式，默认进入监控页 */
    app_ui_mode_t ui_mode = APP_UI_MODE_MONITOR;

    /* 启动前清零本地缓存 */
    memset(&snap, 0, sizeof(snap));

    /* 初始化按键驱动 */
    bsp_key_init();

    /* 初始化 UI 服务 */
    svc_ui_init();

    /* 将初始 UI 模式同步到全局数据区 */
    app_data_set_ui_mode(ui_mode);

    for (;;)
    {
        /* 以 10ms 基准扫描按键状态 */
        bsp_key_scan_10ms();

        /* 获取本轮按键触发事件（边沿触发） */
        key_trg = bsp_key_trg_get();

        /* PREV 键：触发一次确认提示音 */
        if ((key_trg & KEY_MASK_PREV) != 0U)
        {
            app_ctrl_request_buzz_ack();
        }

        /* MODE 键：切换页面模式，并强制刷新显示 */
        if ((key_trg & KEY_MASK_MODE) != 0U)
        {
            ui_mode = app_ui_mode_next(ui_mode);
            app_data_set_ui_mode(ui_mode);
            svc_ui_force_refresh();
        }

        /* NEXT 键：当前设计中用于请求立即刷新界面 */
        if ((key_trg & KEY_MASK_NEXT) != 0U)
        {
            svc_ui_force_refresh();
        }

        /* 按扫描周期累计，达到刷新周期后执行一次界面渲染 */
        refresh_cnt += APP_TASK_UI_SCAN_PERIOD_MS;
        if (refresh_cnt >= APP_TASK_UI_REFRESH_MS)
        {
            refresh_cnt = 0U;

            /* 读取当前系统快照，作为界面显示输入 */
            app_data_get_snapshot(&snap);

            /* 渲染当前 UI 页面 */
            svc_ui_render(&snap);
        }

        /* 使用绝对时基延时，减小任务调度抖动 */
        next_wake += APP_TASK_UI_SCAN_PERIOD_MS;
        (void)osDelayUntil(next_wake);
    }
}