/**
 * @file svc_ui.c
 * @brief UI 渲染服务实现
 */

#include "svc_ui.h"

#include <stdio.h>
#include <string.h>

#include "drv_oled.h"
#include "app_types.h"

/** @brief 第 0 行 Y 坐标 */
#define UI_Y0   0

/** @brief 第 1 行 Y 坐标 */
#define UI_Y1   16

/** @brief 第 2 行 Y 坐标 */
#define UI_Y2   32

/** @brief 第 3 行 Y 坐标 */
#define UI_Y3   48

/** @brief 强制刷新标志 */
static uint8_t s_force_refresh = 1U;

/**
 * @brief 计算整数绝对值
 * @param v 输入整数
 * @return 绝对值
 */
static int app_abs_i(int v)
{
    return (v < 0) ? -v : v;
}

/**
 * @brief 将 x10 定点数转换为字符串
 * @param[in]  x10    输入值，单位 x10
 * @param[out] out    输出字符串缓冲区
 * @param[in]  out_sz 输出缓冲区大小
 * @return 无
 * @note 例如：253 -> "25.3"，-18 -> "-1.8"
 */
static void x10_to_str(int x10, char *out, uint32_t out_sz)
{
    int ip = x10 / 10;
    int fp = app_abs_i(x10 % 10);

    (void)snprintf(out, out_sz, "%d.%d", ip, fp);
}

/**
 * @brief 获取当前告警状态对应的显示字符串
 * @param[in] snap 系统快照数据
 * @return 告警状态字符串
 * @note 按优先级依次显示 DRY / RAIN / NET / NORMAL
 */
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

/**
 * @brief 绘制页面标题
 * @param[in] title 标题字符串
 * @return 无
 */
static void draw_title(const char *title)
{
    OLED_Printf(0, UI_Y0, OLED_8X16, "  %s", (char *)title);
}

/**
 * @brief 绘制监控页面
 * @param[in] snap 系统快照数据
 * @return 无
 * @note 显示温湿度、光照、降雨、土壤湿度和告警状态
 */
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

/**
 * @brief 绘制系统状态页面
 * @param[in] snap 系统快照数据
 * @return 无
 * @note 显示 Wi-Fi、MQTT、告警状态、RSSI 和发布计数
 */
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

/**
 * @brief 绘制设备信息页面
 * @param[in] snap 系统快照数据
 * @return 无
 * @note 显示运行时长、网络状态和系统序号
 */
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

/**
 * @brief 初始化 UI 服务
 * @return 无
 * @note 完成 OLED 初始化、清屏及初始提示显示
 */
void svc_ui_init(void)
{
    OLED_Init();
    OLED_Clear();
    OLED_Printf(0, 0, OLED_8X16, "UI INIT...");
    OLED_Update();

    s_force_refresh = 1U;
}

/**
 * @brief 请求强制刷新界面
 * @return 无
 */
void svc_ui_force_refresh(void)
{
    s_force_refresh = 1U;
}

/**
 * @brief 根据系统快照渲染当前页面
 * @param[in] snap 系统快照数据
 * @return 无
 * @note 根据 ui_mode 选择不同页面模板进行绘制
 */
void svc_ui_render(const app_snapshot_t *snap)
{
    if (snap == NULL)
    {
        return;
    }

    /* 每次渲染前先清屏，再按页面模式重新绘制 */
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