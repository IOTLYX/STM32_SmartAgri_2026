#include "drv_key.h"

/* 内部状态 */
static uint8_t s_trg = 0;          // 按下沿触发（一次性事件）
static uint8_t s_cont = 0;         // 稳定按住状态（持续）
static uint8_t s_last_raw = 0xFF;  // 上一次原始电平打包
static uint8_t s_stable_raw = 0xFF;// 稳定后的原始电平打包
static uint8_t s_cnt = 0;          // 去抖计数

/* 读原始电平并打包：
 * bit0=PREV  bit1=NEXT  bit2=MODE
 * raw 的 bit 值是“引脚电平”（0/1），不是“按下/松开”
 */
static uint8_t _read_raw_pack(void)
{
    uint8_t raw = 0;
    raw |= ((KEY_READ_PREV() != GPIO_PIN_RESET) ? 1u : 0u) << 0;
    raw |= ((KEY_READ_NEXT() != GPIO_PIN_RESET) ? 1u : 0u) << 1;
    raw |= ((KEY_READ_MODE() != GPIO_PIN_RESET) ? 1u : 0u) << 2;
    return raw & 0x07u;
}

/* 将 raw 电平转成 press 位图：1=按下 */
static uint8_t _raw_to_press(uint8_t raw)
{
#if (KEY_ACTIVE_LEVEL == 1)
    return (uint8_t)(raw & 0x07u);         // 高电平按下：press=raw
#else
    return (uint8_t)((~raw) & 0x07u);      // 低电平按下：press=~raw
#endif
}

void key_init(void)
{
    s_trg = 0;
    s_cnt = 0;

    s_last_raw   = _read_raw_pack();
    s_stable_raw = s_last_raw;

    s_cont = _raw_to_press(s_stable_raw);
}

void key_scan_10ms(void)
{
    uint8_t raw = _read_raw_pack();

    /* 去抖：连续 KEY_DEBOUNCE_CNT 次相同，才认为稳定 */
    if (raw == s_last_raw) {
        if (s_cnt < KEY_DEBOUNCE_CNT) s_cnt++;
    } else {
        s_cnt = 0;
        s_last_raw = raw;
    }

    if (s_cnt == KEY_DEBOUNCE_CNT && s_stable_raw != raw) {
        s_stable_raw = raw;

        uint8_t press = _raw_to_press(s_stable_raw);

        /* 触发：只在“0->1(按下)”那一刻置位一次 */
        s_trg  = (uint8_t)(press & (press ^ s_cont));
        s_cont = press;
    } else {
        s_trg = 0;
    }
}

uint8_t key_trg_get(void)
{
    uint8_t t = s_trg;
    s_trg = 0;
    return t;
}

uint8_t key_cont_get(void)
{
    return s_cont;
}
