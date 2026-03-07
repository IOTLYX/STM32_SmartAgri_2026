#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_MODE_MONITOR = 0,  // 监测模式（默认）
    UI_MODE_CONTROL = 1,  // 控制模式（手动/动作提示）
    UI_MODE_SETUP   = 2,  // 设置模式（阈值/校准/参数）
    UI_MODE_DEBUG   = 3,  // 调试模式（原始值/计数/版本）
    UI_MODE_COUNT
} ui_mode_t;

/* UI 数据快照：你后面采样/联网得到的数据都填进来 */
typedef struct {
    int16_t  temp_x10;     // 例如 253 表示 25.3℃
    int16_t  hum_x10;      // 例如 612 表示 61.2%
    uint16_t lux;          // 光照
    uint8_t  rain_pct;     // 0~100
    uint8_t  soil_pct;  // 0~100（没有的可以先填0）

    /* 联网/状态（可选，没做就填0/空） */
    uint8_t  wifi_ok;      // 0/1
    int16_t  rssi;         // dBm（如 -55），没有填 0
    uint32_t up_cnt;       // 上传次数（可选）
    uint8_t  alarm_on;     // 0/1（是否报警）
} app_ui_data_t;

/* 初始化 UI（内部会 OLED_Init 并显示默认页） */
void app_ui_init(void);

/* 每 500ms/200ms 刷新一次（推荐放到你的 500ms 任务里） */
void app_ui_refresh(void);

/* 有新数据时喂给 UI（会自动标记“需要重绘”） */
void app_ui_set_data(const app_ui_data_t *d);

/* 处理按键触发位图（用 bsp_key_trg_get() 的返回值直接喂进来） */
void app_ui_on_key_trg(uint8_t trg_mask);

/* 如果你想在别处直接切模式/翻页，也提供接口 */
ui_mode_t app_ui_mode_get(void);
void      app_ui_mode_set(ui_mode_t m);
void      app_ui_page_next(void);
void      app_ui_page_prev(void);
void      app_ui_mark_dirty(void);

#ifdef __cplusplus
}
#endif
