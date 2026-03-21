#include "svc_alarm.h"
#include "app_cfg.h"

#include <string.h>
#include "cmsis_os2.h"

#include "bsp_buzz.h"
#include "bsp_led.h"

static uint8_t s_dry_alarm = 0U;
static uint8_t s_rain_alarm = 0U;
static uint8_t s_net_lost_alarm = 0U;

static uint8_t s_buzz_muted = 0U;
static uint8_t s_buzz_muted_mode = BUZZ_MODE_OFF;

static uint8_t s_net_ever_ok = 0U;
static uint32_t s_net_bad_since = 0U;

static uint8_t s_ack_req = 0U;

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

void svc_alarm_ack_toggle(void)
{
    s_ack_req = 1U;
}

void svc_alarm_process(const app_snapshot_t *snap, app_alarm_data_t *alarm)
{
    if ((snap == NULL) || (alarm == NULL))
    {
        return;
    }

    memset(alarm, 0, sizeof(*alarm));

    if (snap->net.wifi_ok != 0U)
    {
        s_net_ever_ok = 1U;
        s_net_bad_since = 0U;
        s_net_lost_alarm = 0U;
    }
    else
    {
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

    s_dry_alarm = hyst_low_on_high_off(s_dry_alarm,
                                       snap->env.soil_pct,
                                       APP_TH_SOIL_DRY_ON_PCT,
                                       APP_TH_SOIL_DRY_OFF_PCT);

    s_rain_alarm = hyst_high_on_low_off(s_rain_alarm,
                                        snap->env.rain_pct,
                                        APP_TH_RAIN_ON_PCT,
                                        APP_TH_RAIN_OFF_PCT);

    alarm->soil_dry = s_dry_alarm;
    alarm->raining  = s_rain_alarm;
    alarm->net_lost = s_net_lost_alarm;
    alarm->alarm_on = (uint8_t)((s_dry_alarm != 0U) ||
                                (s_rain_alarm != 0U) ||
                                (s_net_lost_alarm != 0U));

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

    if (s_ack_req != 0U)
    {
        s_ack_req = 0U;

        if (alarm->buzz_mode == BUZZ_MODE_OFF)
        {
            s_buzz_muted = 0U;
            s_buzz_muted_mode = BUZZ_MODE_OFF;
        }
        else if (s_buzz_muted == 0U)
        {
            s_buzz_muted = 1U;
            s_buzz_muted_mode = alarm->buzz_mode;
        }
        else
        {
            s_buzz_muted = 0U;
            s_buzz_muted_mode = BUZZ_MODE_OFF;
        }
    }

    if (alarm->buzz_mode == BUZZ_MODE_OFF)
    {
        s_buzz_muted = 0U;
        s_buzz_muted_mode = BUZZ_MODE_OFF;
    }

    if ((s_buzz_muted != 0U) && (alarm->buzz_mode != s_buzz_muted_mode))
    {
        s_buzz_muted = 0U;
        s_buzz_muted_mode = BUZZ_MODE_OFF;
    }

    alarm->buzz_muted = s_buzz_muted;
}