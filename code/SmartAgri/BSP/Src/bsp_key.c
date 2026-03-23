#include "bsp_key.h"
#include "main.h"   /* KEY_xxx_Pin / KEY_xxx_GPIO_Port */

/* 按键内部状态变量 */
static uint8_t s_trg = 0;          /* 按键触发标志：仅在按下边沿置位一次 */
static uint8_t s_cont = 0;         /* 当前稳定按下状态 */

static uint8_t s_last_raw = 0xFF;  /* 上一次原始采样值 */
static uint8_t s_stable_raw = 0xFF;/* 当前稳定原始值 */
static uint8_t s_cnt = 0;          /* 消抖计数器 */

/**
 * @brief 读取原始按键电平并打包
 *
 * bit0 = PREV
 * bit1 = NEXT
 * bit2 = MODE
 *
 * 当前按键定义为 Active High，因此读到高电平表示按下。
 *
 * @return uint8_t 原始按键位图
 */
static uint8_t _read_raw_pack(void)
{
    uint8_t raw = 0;

    raw |= (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) != GPIO_PIN_RESET ? 1u : 0u) << 0;
    raw |= (HAL_GPIO_ReadPin(KEY_2_GPIO_Port, KEY_2_Pin) != GPIO_PIN_RESET ? 1u : 0u) << 1;
    raw |= (HAL_GPIO_ReadPin(KEY_3_GPIO_Port, KEY_3_Pin) != GPIO_PIN_RESET ? 1u : 0u) << 2;

    return (uint8_t)(raw & 0x07u);
}

/**
 * @brief 将原始电平转换为按下状态
 *
 * 当前设计中按键为 Active High，
 * 因此原始值可直接作为按下状态返回。
 *
 * @param raw 原始按键位图
 * @return uint8_t 按下状态位图
 */
static uint8_t _raw_to_press(uint8_t raw)
{
    return (uint8_t)(raw & 0x07u);
}

/**
 * @brief 初始化按键驱动
 *
 * 读取当前按键电平作为初始稳定状态，
 * 避免上电瞬间产生误触发。
 */
void bsp_key_init(void)
{
    s_trg = 0;
    s_cnt = 0;

    /* 上电先读取一次当前电平，作为初始状态 */
    s_last_raw   = _read_raw_pack();
    s_stable_raw = s_last_raw;

    /* 初始化当前稳定按下状态 */
    s_cont = _raw_to_press(s_stable_raw);
}

/**
 * @brief 按键 10ms 扫描函数
 *
 * 通过连续多次一致判断实现消抖，
 * 当检测到稳定状态变化时，更新按下状态和触发标志。
 */
void bsp_key_scan_10ms(void)
{
    uint8_t raw = _read_raw_pack();

    if (raw == s_last_raw)
    {
        /* 连续采样一致，则累加消抖计数 */
        if (s_cnt < BSP_KEY_DEBOUNCE_CNT)
        {
            s_cnt++;
        }
    }
    else
    {
        /* 原始值变化，重新开始消抖计数 */
        s_cnt = 0;
        s_last_raw = raw;
    }

    if ((s_cnt == BSP_KEY_DEBOUNCE_CNT) && (s_stable_raw != raw))
    {
        s_stable_raw = raw;

        /* 转换为稳定按下状态 */
        uint8_t press = _raw_to_press(s_stable_raw);

        /* 仅在“新按下”的上升沿产生一次触发 */
        s_trg  = (uint8_t)(press & (press ^ s_cont));
        s_cont = press;
    }
    else
    {
        /* 非新触发时清零单次触发标志 */
        s_trg = 0;
    }
}

/**
 * @brief 获取按键触发标志
 *
 * 返回“刚按下”的触发状态，读取后自动清零。
 *
 * @return uint8_t 触发位图
 */
uint8_t bsp_key_trg_get(void)
{
    uint8_t t = s_trg;
    s_trg = 0;
    return t;
}

/**
 * @brief 获取当前稳定按下状态
 *
 * 返回当前经过消抖后的按下状态，不清零。
 *
 * @return uint8_t 当前稳定按下位图
 */
uint8_t bsp_key_cont_get(void)
{
    return s_cont;
}