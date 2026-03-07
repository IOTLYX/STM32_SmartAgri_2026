#include "app_ui.h"
#include <string.h>
#include <stdio.h>

#include "drv_oled.h"
#include "bsp_key.h"   // KEY_MASK_PREV / NEXT / MODE

#define Y0  0
#define Y1  16
#define Y2  32
#define Y3  48

/* ===================== 阈值（仅用于右下角报警词判定） ===================== */
/* 缺水阈值：土壤百分比 <= 这个值判为“缺水” */
#define SOIL_DRY_TH_PCT     30u
/* 下雨阈值：雨滴百分比 >= 这个值判为“下雨” */
#define RAIN_ON_TH_PCT      80u

/* ===================== 内部状态 ===================== */
static app_ui_data_t s_dat;

/* 模式：
 * UI_MODE_MONITOR -> 环境监测
 * UI_MODE_CONTROL -> 系统状态
 * UI_MODE_DEBUG   -> 系统信息
 * UI_MODE_SETUP   -> 不使用（跳过）
 */
static ui_mode_t s_mode = UI_MODE_MONITOR;
static uint8_t s_dirty = 1;

/* ===================== 右下角报警词：锁存/确认隐藏 ===================== */
typedef enum {
    ALM_NONE = 0,
    ALM_NET_DOWN,
    ALM_SOIL_DRY,
    ALM_RAINING,
    ALM_MULTI
} alm_code_t;

static alm_code_t s_alm_last = ALM_NONE;  /* 最近一次检测到的“当前报警类型” */
static uint8_t    s_alm_acked = 0;        /* 用户是否已确认当前报警（确认后隐藏） */

/* ===================== 工具函数 ===================== */
static int _abs_i(int v) { return (v < 0) ? -v : v; }

static void _x10_to_1dp(int x10, char *out, unsigned out_sz)
{
    int ip = x10 / 10;
    int fp = _abs_i(x10 % 10);
    (void)snprintf(out, out_sz, "%d.%d", ip, fp);
}

/* 你现在只有一个 soil_pct，直接用它 */
static uint8_t _soil_pct(void)
{
    return s_dat.soil_pct;  // 0~100
}

static alm_code_t _calc_alarm_code(uint8_t soil_pct)
{
    uint8_t net_down = (s_dat.wifi_ok ? 0u : 1u);
    uint8_t soil_dry = (soil_pct <= SOIL_DRY_TH_PCT) ? 1u : 0u;
    uint8_t raining  = (s_dat.rain_pct >= RAIN_ON_TH_PCT) ? 1u : 0u;

    uint8_t cnt = (uint8_t)(net_down + soil_dry + raining);
    if (cnt >= 2) return ALM_MULTI;
    if (net_down) return ALM_NET_DOWN;
    if (soil_dry) return ALM_SOIL_DRY;
    if (raining)  return ALM_RAINING;
    return ALM_NONE;
}

static const char* _alm_word(alm_code_t c)
{
    switch (c) {
    default:
    case ALM_NONE:     return "正常";
    case ALM_NET_DOWN: return "网断";
    case ALM_SOIL_DRY: return "缺水";
    case ALM_RAINING:  return "下雨";
    case ALM_MULTI:    return "多异常";
    }
}

static ui_mode_t _next_mode(ui_mode_t cur)
{
    if (cur == UI_MODE_MONITOR) return UI_MODE_CONTROL;
    if (cur == UI_MODE_CONTROL) return UI_MODE_DEBUG;
    return UI_MODE_MONITOR;
}

/* ===================== 标题行（简单居中效果） ===================== */
static void _draw_title_center(const char *title_cn)
{
    OLED_Printf(0, Y0, OLED_8X16, "  %s", title_cn);
}

/* ===================== 模式1：环境监测 ===================== */
static void _draw_mode_monitor(void)
{
    char t[12], h[12];
    _x10_to_1dp(s_dat.temp_x10, t, sizeof(t));
    _x10_to_1dp(s_dat.hum_x10,  h, sizeof(h));

    uint8_t soil = _soil_pct();
    alm_code_t now = _calc_alarm_code(soil);

    /* 锁存/确认逻辑：
     * - 报警类型变化：认为出现“新情况”，清掉确认标志，让提示重新出现
     * - 无异常：自动清掉确认标志
     * - 有异常且已确认：隐藏提示，显示“正常”（按你的要求）
     */
    if (now != s_alm_last) {
        s_alm_last  = now;
        s_alm_acked = 0;
    }
    if (now == ALM_NONE) {
        s_alm_acked = 0;
    }

    const char *word = "正常";
    if ((now != ALM_NONE) && (s_alm_acked == 0)) {
        word = _alm_word(now);
    }

    _draw_title_center("环 境 监 测");
    OLED_Printf(0, Y1, OLED_8X16, "温:%-4s 湿:%s", t, h);
    OLED_Printf(0, Y2, OLED_8X16, "光:%-4u 雨:%u%%", (unsigned)s_dat.lux, (unsigned)s_dat.rain_pct);
    OLED_Printf(0, Y3, OLED_8X16, "土:%u%%  %s", (unsigned)soil, word);
}

/* ===================== 模式2：系统状态（显示真实状态，不受确认隐藏影响） ===================== */
static void _draw_mode_status(void)
{
    const char *net = s_dat.wifi_ok ? "已连接" : "已断开";
    const char *alm = s_dat.alarm_on ? "已触发" : "无异常";
    const char *run = (s_dat.wifi_ok ? "正常" : "受限");

    _draw_title_center("系 统 状 态");
    OLED_Printf(0, Y1, OLED_8X16, "网络:%s", net);
    OLED_Printf(0, Y2, OLED_8X16, "报警:%s", alm);
    OLED_Printf(0, Y3, OLED_8X16, "运行:%s", run);
}

/* ===================== 模式3：系统信息 ===================== */
static void _draw_mode_info(void)
{
    _draw_title_center("系 统 信 息");
    OLED_Printf(0, Y1, OLED_8X16, "设备:智慧农田");
    OLED_Printf(0, Y2, OLED_8X16, "版本:v1.0");
    OLED_Printf(0, Y3, OLED_8X16, "通信:WiFi");
}

/* ===================== 当前绘制 ===================== */
static void _draw_current(void)
{
    OLED_Clear();

    switch (s_mode) {
    default:
    case UI_MODE_MONITOR: _draw_mode_monitor(); break;
    case UI_MODE_CONTROL: _draw_mode_status();  break;
    case UI_MODE_DEBUG:   _draw_mode_info();    break;

    case UI_MODE_SETUP:
        s_mode = UI_MODE_MONITOR;
        _draw_mode_monitor();
        break;
    }

    OLED_Update();
}

/* ===================== 对外接口 ===================== */
void app_ui_mark_dirty(void) { s_dirty = 1; }
ui_mode_t app_ui_mode_get(void) { return s_mode; }

void app_ui_mode_set(ui_mode_t m)
{
    if (m == UI_MODE_SETUP) m = UI_MODE_MONITOR;
    if (m >= UI_MODE_COUNT) m = UI_MODE_MONITOR;

    if (s_mode != m) {
        s_mode = m;
        s_dirty = 1;
    }
}

/* 保留分页函数但不做分页 */
void app_ui_page_next(void) { s_dirty = 1; }
void app_ui_page_prev(void) { /* 空 */ }

void app_ui_set_data(const app_ui_data_t *d)
{
    if (!d) return;
    s_dat = *d;
    s_dirty = 1;
}

void app_ui_on_key_trg(uint8_t trg_mask)
{
    /* PREV：确认/消警（只影响监测页右下角提示词显示） */
    if (trg_mask & KEY_MASK_PREV) {
        s_alm_acked = 1;
        s_dirty = 1;
    }

    /* NEXT：手动刷新 */
    if (trg_mask & KEY_MASK_NEXT) {
        s_dirty = 1;
    }

    /* MODE：切模式（3模式循环） */
    if (trg_mask & KEY_MASK_MODE) {
        app_ui_mode_set(_next_mode(s_mode));
    }
}

void app_ui_init(void)
{
    memset(&s_dat, 0, sizeof(s_dat));
    s_mode = UI_MODE_MONITOR;
    s_dirty = 1;

    s_alm_last = ALM_NONE;
    s_alm_acked = 0;

    OLED_Init();
    OLED_Clear();
    OLED_Printf(0, 0, OLED_8X16, "UI init...");
    OLED_Update();
}

void app_ui_refresh(void)
{
    if (!s_dirty) return;
    s_dirty = 0;
    _draw_current();
}
