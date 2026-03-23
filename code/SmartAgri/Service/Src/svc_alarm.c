/**
 * @file svc_alarm.c
 * @brief 告警服务实现
 */

#include "svc_alarm.h"
#include "app_cfg.h"

#include <string.h>
#include "cmsis_os2.h"

#include "bsp_buzz.h"
#include "bsp_led.h"

/** @brief 土壤干燥告警状态 */
static uint8_t s_dry_alarm = 0U;

/** @brief 降雨告警状态 */
static uint8_t s_rain_alarm = 0U;

/** @brief 网络断连告警状态 */
static uint8_t s_net_lost_alarm = 0U;

/** @brief 蜂鸣器静音状态 */
static uint8_t s_buzz_muted = 0U;

/** @brief 当前被静音的蜂鸣模式 */
static uint8_t s_buzz_muted_mode = BUZZ_MODE_OFF;

/** @brief 网络是否曾经连通过 */
static uint8_t s_net_ever_ok = 0U;

/** @brief 网络异常开始时刻 */
static uint32_t s_net_bad_since = 0U;

/** @brief 告警确认请求标志 */
static uint8_t s_ack_req = 0U;

/**
 * @brief 低值触发、高值解除的滞回比较
 * @param st 当前状态
 * @param val 当前输入值
 * @param on_th 触发阈值（小于等于该值触发）
 * @param off_th 解除阈值（大于等于该值解除）
 * @return 更新后的状态
 * @note 适用于“数值越小越危险”的场景，如土壤湿度过低
 */
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

/**
 * @brief 高值触发、低值解除的滞回比较
 * @param st 当前状态
 * @param val 当前输入值
 * @param on_th 触发阈值（大于等于该值触发）
 * @param off_th 解除阈值（小于等于该值解除）
 * @return 更新后的状态
 * @note 适用于“数值越大越危险”的场景，如雨量过高
 */
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

/**
 * @brief 初始化告警服务内部状态
 * @return 无
 */
void svc_alarm_init(void)
{
    s_dry_alarm = 0U;
    s_rain_alarm = 0U;
    s_net_lost_alarm = 0U;

    s_buzz_muted = 0U;
    s_buzz_muted_mode = BUZZ_MODE_OFF;

    s_net_ever_ok = 0U;
    s_net_bad_since = 0U;
    s_ack_req = 0U;
}

/**
 * @brief 请求切换告警确认/静音状态
 * @return 无
 * @note 实际处理在 svc_alarm_process() 中完成
 */
void svc_alarm_ack_toggle(void)
{
    s_ack_req = 1U;
}

/**
 * @brief 根据系统快照更新告警结果
 * @param[in]  snap  当前系统快照
 * @param[out] alarm 告警输出结果
 * @return 无
 * @note 处理内容包括：
 *       1. 网络断连延时告警
 *       2. 土壤干燥与降雨滞回判断
 *       3. 蜂鸣器模式与静音逻辑
 *       4. 告警指示灯模式选择
 */
void svc_alarm_process(const app_snapshot_t *snap, app_alarm_data_t *alarm)
{
    if ((snap == NULL) || (alarm == NULL))
    {
        return;
    }

    /* 先清空输出结构，避免残留旧状态 */
    memset(alarm, 0, sizeof(*alarm));

    /* 网络告警采用延时触发，避免瞬时抖动误报 */
    if (snap->net.wifi_ok != 0U)
    {
        s_net_ever_ok = 1U;
        s_net_bad_since = 0U;
        s_net_lost_alarm = 0U;
    }
    else
    {
        /* 仅在曾经联网成功后，才开始统计掉线时间 */
        if ((s_net_ever_ok != 0U) && (s_net_bad_since == 0U))
        {
            s_net_bad_since = osKernelGetTickCount();
        }

        if ((s_net_bad_since != 0U) &&
            ((osKernelGetTickCount() - s_net_bad_since) >= APP_NET_LOST_ALM_DELAY_MS))
        {
            s_net_lost_alarm = 1U;
        }
    }

    /* 土壤湿度：低于阈值触发，高于恢复阈值解除 */
    s_dry_alarm = hyst_low_on_high_off(s_dry_alarm,
                                       snap->env.soil_pct,
                                       APP_TH_SOIL_DRY_ON_PCT,
                                       APP_TH_SOIL_DRY_OFF_PCT);

    /* 降雨强度：高于阈值触发，低于恢复阈值解除 */
    s_rain_alarm = hyst_high_on_low_off(s_rain_alarm,
                                        snap->env.rain_pct,
                                        APP_TH_RAIN_ON_PCT,
                                        APP_TH_RAIN_OFF_PCT);

    /* 汇总当前告警状态 */
    alarm->soil_dry = s_dry_alarm;
    alarm->raining  = s_rain_alarm;
    alarm->net_lost = s_net_lost_alarm;
    alarm->alarm_on = (uint8_t)((s_dry_alarm != 0U) ||
                                (s_rain_alarm != 0U) ||
                                (s_net_lost_alarm != 0U));

    /* 按优先级选择蜂鸣器和告警灯模式 */
    if (s_dry_alarm != 0U)
    {
        alarm->buzz_mode = BUZZ_MODE_DRY;
        alarm->led_alarm_mode = LED_ALM_DRY;
    }
    else if (s_rain_alarm != 0U)
    {
        alarm->buzz_mode = BUZZ_MODE_RAIN;
        alarm->led_alarm_mode = LED_ALM_RAIN;
    }
    else if (s_net_lost_alarm != 0U)
    {
        alarm->buzz_mode = BUZZ_MODE_NET_LOST;
        alarm->led_alarm_mode = LED_ALM_NONE;
    }
    else
    {
        alarm->buzz_mode = BUZZ_MODE_OFF;
        alarm->led_alarm_mode = LED_ALM_NONE;
    }

    /* 处理用户确认/静音请求 */
    if (s_ack_req != 0U)
    {
        s_ack_req = 0U;

        if (alarm->buzz_mode == BUZZ_MODE_OFF)
        {
            /* 当前无告警时，清除静音状态 */
            s_buzz_muted = 0U;
            s_buzz_muted_mode = BUZZ_MODE_OFF;
        }
        else if (s_buzz_muted == 0U)
        {
            /* 当前有告警且未静音，则进入静音 */
            s_buzz_muted = 1U;
            s_buzz_muted_mode = alarm->buzz_mode;
        }
        else
        {
            /* 已静音时，再次确认可取消静音 */
            s_buzz_muted = 0U;
            s_buzz_muted_mode = BUZZ_MODE_OFF;
        }
    }

    /* 无告警时自动退出静音状态 */
    if (alarm->buzz_mode == BUZZ_MODE_OFF)
    {
        s_buzz_muted = 0U;
        s_buzz_muted_mode = BUZZ_MODE_OFF;
    }

    /* 若告警类型发生变化，取消旧静音，确保新告警能重新提示 */
    if ((s_buzz_muted != 0U) && (alarm->buzz_mode != s_buzz_muted_mode))
    {
        s_buzz_muted = 0U;
        s_buzz_muted_mode = BUZZ_MODE_OFF;
    }

    alarm->buzz_muted = s_buzz_muted;
}