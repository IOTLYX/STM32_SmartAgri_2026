#include "app_task_net.h"
#include "app_data.h"
#include "app_cfg.h"

#include <string.h>
#include "cmsis_os2.h"

#include "usart.h"
#include "svc_net.h"
#include "bsp_log.h"

/**
 * @brief 网络任务入口函数
 *
 * 该任务负责系统网络通信相关逻辑，主要包括：
 * 1. 初始化日志串口和网络服务模块
 * 2. 维护 WiFi / MQTT 连接状态
 * 3. 在网络异常时按冷却时间间隔执行重连
 * 4. 在网络正常时周期性发布系统快照数据
 * 5. 统计发布成功/失败次数，并在连续失败后触发掉线重连
 *
 * @param argument 任务入口参数，当前未使用
 */
void netTaskStart(void *argument)
{
    (void)argument;

    /* 记录下次任务唤醒时刻，用于实现固定周期调度 */
    uint32_t next_wake = osKernelGetTickCount();

    /* 记录上次尝试重连时间，用于限制重试频率 */
    uint32_t last_try_ms = 0U;

    /* 本地快照缓存和网络状态缓存 */
    app_snapshot_t snap;
    app_net_data_t net;

    /* 先清零本地变量，避免使用未初始化数据 */
    memset(&snap, 0, sizeof(snap));
    memset(&net, 0, sizeof(net));

    /* 初始化日志输出串口 */
    bsp_log_init(&huart1);

    /* 初始化网络服务，通常一个串口连通信模组，一个串口做调试日志 */
    svc_net_init(&huart2, &huart1);

    /* netTask 首次运行先延时一段时间，给底层模组上电和启动留余量 */
    static uint8_t s_net_boot_wait_done = 0U;

    if (s_net_boot_wait_done == 0U)
    {
        s_net_boot_wait_done = 1U;
        osDelay(1000U);
    }

    for (;;)
    {
        /* 当 WiFi 或 MQTT 任一未连接时，进入建链/重连流程 */
        if ((net.wifi_ok == 0U) || (net.mqtt_ok == 0U))
        {
            uint32_t now = HAL_GetTick();

            /* 首次执行或距离上次重试达到冷却时间后，才允许再次重连 */
            if ((last_try_ms == 0U) ||
                ((uint32_t)(now - last_try_ms) >= APP_NET_RETRY_COOLDOWN_MS))
            {
                last_try_ms = now;

                /* 尝试拉起网络连接状态 */
                (void)svc_net_ensure_up(&net);

                /* 将最新网络状态同步到全局数据区 */
                app_data_set_net(&net);
            }
        }
        else
        {
            /* 网络正常时，先读取当前系统快照作为上报输入 */
            app_data_get_snapshot(&snap);

            /* 发布当前系统快照；成功则清失败计数，失败则累计统计 */
            if (svc_net_publish_snapshot(&snap, &net) == true)
            {
                net.pub_ok_cnt++;
                net.pub_fail_streak = 0U;
            }
            else
            {
                net.pub_fail_cnt++;

                /* 连续失败次数做饱和计数，防止 8bit 计数溢出回绕 */
                if (net.pub_fail_streak < 255U)
                {
                    net.pub_fail_streak++;
                }

                /* 连续失败达到阈值后，主动标记掉线，交由下一轮进入重连流程 */
                if (net.pub_fail_streak >= 2U)
                {
                    net.pub_fail_streak = 0U;
                    net.wifi_ok = 0U;
                    net.mqtt_ok = 0U;
                }
            }

            /* 发布完成后，将最新网络状态写回全局数据区 */
            app_data_set_net(&net);
        }

        /* 使用绝对时基延时，减小任务周期漂移 */
        next_wake += APP_TASK_NET_PERIOD_MS;
        (void)osDelayUntil(next_wake);
    }
}