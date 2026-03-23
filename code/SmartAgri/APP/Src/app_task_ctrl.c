#include "app_task_ctrl.h"
#include "app_data.h"
#include "app_cfg.h"

#include <string.h>
#include "cmsis_os2.h"

#include "bsp_led.h"
#include "bsp_buzz.h"
#include "tim.h"
#include "svc_alarm.h"

/**
 * @brief 请求蜂鸣器播放确认提示音
 *
 * 该接口用于响应用户确认类操作，
 * 实际上通过切换告警确认状态来触发提示音逻辑。
 */
void app_ctrl_request_buzz_ack(void)
{
    /* 触发一次告警确认/应答动作 */
    svc_alarm_ack_toggle();
}

/**
 * @brief 控制任务的一次性初始化
 *
 * 负责初始化控制任务依赖的底层外设和服务模块，
 * 只应在任务启动时调用一次。
 */
static void app_ctrl_init_once(void)
{
    /* 初始化 LED 指示灯驱动 */
    bsp_led_init();

    /* 初始化蜂鸣器驱动，使用定时器3通道1输出 PWM */
    bsp_buzz_init(&htim3, TIM_CHANNEL_1);

    /* 初始化告警服务模块 */
    svc_alarm_init();
}

/**
 * @brief 控制任务入口函数
 *
 * 该任务作为系统主控制循环，周期性执行以下功能：
 * 1. 驱动 LED 和蜂鸣器的非阻塞状态机
 * 2. 获取系统当前快照数据
 * 3. 执行告警状态处理
 * 4. 更新蜂鸣器和 LED 指示状态
 * 5. 维护系统运行时间
 * 6. 执行心跳灯翻转
 *
 * @param argument 任务参数，当前未使用
 */
void ctrlTaskStart(void *argument)
{
    (void)argument;

    /* 记录下次唤醒时刻，用于实现固定周期调度 */
    uint32_t next_wake = osKernelGetTickCount();

    /* 记录上次更新时间戳，用于按 1s 统计运行时间 */
    uint32_t last_uptime_tick = osKernelGetTickCount();

    /* 系统累计运行秒数 */
    uint32_t uptime_s = 0U;

    /* 心跳灯分频计数，每 50 个周期翻转一次 */
    uint8_t heart_div = 0U;

    /* 系统快照与告警处理结果缓存 */
    app_snapshot_t snap;
    app_alarm_data_t alarm;

    /* 启动前先清零本地缓存，避免使用脏数据 */
    memset(&snap, 0, sizeof(snap));
    memset(&alarm, 0, sizeof(alarm));

    /* 初始化本任务依赖的底层模块 */
    app_ctrl_init_once();

    for (;;)
    {
        /* 推进 LED 非阻塞状态机，周期基准为 10ms */
        bsp_led_step_10ms();

        /* 推进蜂鸣器非阻塞状态机，周期基准为 10ms */
        bsp_buzz_step_10ms();

        /* 获取当前系统全量快照，作为本轮控制输入 */
        app_data_get_snapshot(&snap);

        /* 基于当前快照执行告警判定与状态更新 */
        svc_alarm_process(&snap, &alarm);

        /* 若已静音，则强制关闭蜂鸣器；否则按告警模式输出 */
        if ((alarm.buzz_muted != 0U) && (alarm.buzz_mode != BUZZ_MODE_OFF))
        {
            bsp_buzz_set_mode(BUZZ_MODE_OFF);
        }
        else
        {
            bsp_buzz_set_mode((buzz_mode_t)alarm.buzz_mode);
        }

        /* 根据网络状态更新网络指示灯 */
        bsp_led_set_net_ok((snap.net.wifi_ok != 0U) ? true : false);

        /* 根据告警状态更新告警灯模式 */
        bsp_led_set_alarm((led_alarm_t)alarm.led_alarm_mode);

        /* 将本轮最新告警状态写回全局数据区 */
        app_data_set_alarm(&alarm);

        /* 每经过 1000ms 更新一次系统运行时间 */
        if ((osKernelGetTickCount() - last_uptime_tick) >= 1000U)
        {
            last_uptime_tick += 1000U;
            uptime_s++;
            app_data_set_uptime(uptime_s);
        }

        /* 心跳灯分频：50 * 任务周期 后翻转一次 */
        heart_div++;
        if (heart_div >= 50U)
        {
            heart_div = 0U;
            bsp_led_toggle(LED_HEART);
        }

        /* 使用绝对时基延时，减小任务周期抖动 */
        next_wake += APP_TASK_CTRL_PERIOD_MS;
        (void)osDelayUntil(next_wake);
    }
}