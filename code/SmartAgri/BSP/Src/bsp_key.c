#include "bsp_key.h"
#include "main.h"   // KEY_xxx_Pin / KEY_xxx_GPIO_Port

/* 内部状态 */
static uint8_t s_trg = 0;
static uint8_t s_cont = 0;

static uint8_t s_last_raw = 0xFF;
static uint8_t s_stable_raw = 0xFF;
static uint8_t s_cnt = 0;

/* 读原始电平并打包：bit0=PREV bit1=NEXT bit2=MODE（raw存电平0/1） */
static uint8_t _read_raw_pack(void)
{
    uint8_t raw = 0;
    raw |= (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) != GPIO_PIN_RESET ? 1u : 0u) << 0;
    raw |= (HAL_GPIO_ReadPin(KEY_2_GPIO_Port, KEY_2_Pin) != GPIO_PIN_RESET ? 1u : 0u) << 1;
    raw |= (HAL_GPIO_ReadPin(KEY_3_GPIO_Port, KEY_3_Pin) != GPIO_PIN_RESET ? 1u : 0u) << 2;
    return (uint8_t)(raw & 0x07u);
}

/* Active High：按下=1，所以 press=raw */
static uint8_t _raw_to_press(uint8_t raw)
{
    return (uint8_t)(raw & 0x07u);
}

void bsp_key_init(void)
{
    s_trg = 0;
    s_cnt = 0;

    s_last_raw   = _read_raw_pack();
    s_stable_raw = s_last_raw;

    s_cont = _raw_to_press(s_stable_raw);
}

void bsp_key_scan_10ms(void)
{
    uint8_t raw = _read_raw_pack();

    if (raw == s_last_raw) {
        if (s_cnt < BSP_KEY_DEBOUNCE_CNT) s_cnt++;
    } else {
        s_cnt = 0;
        s_last_raw = raw;
    }

    if (s_cnt == BSP_KEY_DEBOUNCE_CNT && s_stable_raw != raw) {
        s_stable_raw = raw;

        uint8_t press = _raw_to_press(s_stable_raw);

        /* 只在按下沿触发一次 */
        s_trg  = (uint8_t)(press & (press ^ s_cont));
        s_cont = press;
    } else {
        s_trg = 0;
    }
}

uint8_t bsp_key_trg_get(void)
{
    uint8_t t = s_trg;
    s_trg = 0;
    return t;
}

uint8_t bsp_key_cont_get(void)
{
    return s_cont;
}
