#include "svc_ui.h"

#include <stdio.h>
#include <string.h>

#include "drv_oled.h"
#include "app_types.h"

#define UI_Y0   0
#define UI_Y1   16
#define UI_Y2   32
#define UI_Y3   48

static uint8_t s_force_refresh = 1U;

static int app_abs_i(int v)
{
    return (v < 0) ? -v : v;
}

static void x10_to_str(int x10, char *out, uint32_t out_sz)
{
    int ip = x10 / 10;
    int fp = app_abs_i(x10 % 10);
    (void)snprintf(out, out_sz, "%d.%d", ip, fp);
}

static const char *alarm_word(const app_snapshot_t *snap)
{
    if (snap == NULL)
    {
        return "NORMAL";
    }

    if (snap->alarm.soil_dry != 0U)
    {
        return "DRY";
    }

    if (snap->alarm.raining != 0U)
    {
        return "RAIN";
    }

    if (snap->alarm.net_lost != 0U)
    {
        return "NET";
    }

    return "NORMAL";
}

static void draw_title(const char *title)
{
    OLED_Printf(0, UI_Y0, OLED_8X16, "  %s", (char *)title);
}

static void draw_monitor(const app_snapshot_t *snap)
{
    char t_buf[16];
    char h_buf[16];

    x10_to_str(snap->env.temp_x10, t_buf, sizeof(t_buf));
    x10_to_str((int)snap->env.hum_x10, h_buf, sizeof(h_buf));

    draw_title("MONITOR");
    OLED_Printf(0, UI_Y1, OLED_8X16, "T:%-5s H:%-5s", t_buf, h_buf);
    OLED_Printf(0, UI_Y2, OLED_8X16, "L:%-5lu R:%3u%%",
                (unsigned long)snap->env.lux,
                (unsigned int)snap->env.rain_pct);
    OLED_Printf(0, UI_Y3, OLED_8X16, "S:%3u%% %-6s",
                (unsigned int)snap->env.soil_pct,
                (char *)alarm_word(snap));
}

static void draw_status(const app_snapshot_t *snap)
{
    const char *wifi_str;
    const char *mqtt_str;
    const char *alm_str;

    wifi_str = (snap->net.wifi_ok != 0U) ? "OK " : "BAD";
    mqtt_str = (snap->net.mqtt_ok != 0U) ? "OK " : "BAD";
    alm_str  = (snap->alarm.alarm_on != 0U) ? "ON " : "OFF";

    draw_title("STATUS");
    OLED_Printf(0, UI_Y1, OLED_8X16, "WIFI:%s MQTT:%s", (char *)wifi_str, (char *)mqtt_str);
    OLED_Printf(0, UI_Y2, OLED_8X16, "ALM :%s RSSI:%4d", (char *)alm_str, (int)snap->net.rssi);
    OLED_Printf(0, UI_Y3, OLED_8X16, "PUB :%lu",
                (unsigned long)snap->net.pub_ok_cnt);
}

static void draw_info(const app_snapshot_t *snap)
{
    const char *wifi_str;
    const char *mqtt_str;

    wifi_str = (snap->net.wifi_ok != 0U) ? "OK " : "BAD";
    mqtt_str = (snap->net.mqtt_ok != 0U) ? "OK " : "BAD";

    draw_title("INFO");
    OLED_Printf(0, UI_Y1, OLED_8X16, "UP:%lu s", (unsigned long)snap->uptime_s);
    OLED_Printf(0, UI_Y2, OLED_8X16, "W:%s M:%s", (char *)wifi_str, (char *)mqtt_str);
    OLED_Printf(0, UI_Y3, OLED_8X16, "SEQ:%lu", (unsigned long)snap->seq);
}

void svc_ui_init(void)
{
    OLED_Init();
    OLED_Clear();
    OLED_Printf(0, 0, OLED_8X16, "UI INIT...");
    OLED_Update();
    s_force_refresh = 1U;
}

void svc_ui_force_refresh(void)
{
    s_force_refresh = 1U;
}

void svc_ui_render(const app_snapshot_t *snap)
{
    if (snap == NULL)
    {
        return;
    }

    OLED_Clear();

    switch (snap->ui_mode)
    {
        case APP_UI_MODE_MONITOR:
            draw_monitor(snap);
            break;

        case APP_UI_MODE_STATUS:
            draw_status(snap);
            break;

        case APP_UI_MODE_INFO:
            draw_info(snap);
            break;

        default:
            draw_monitor(snap);
            break;
    }

    OLED_Update();
    s_force_refresh = 0U;
}

