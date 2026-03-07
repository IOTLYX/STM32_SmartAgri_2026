#pragma once
#include <stdint.h>
#include "main.h"   // 用这里面的 PREV_Pin / PREV_GPIO_Port 等

#ifdef __cplusplus
extern "C" {
#endif

    /* ================== 你的硬件约定 ==================
     * 不按 = 0，按下 = 1  => 按下为高电平
     */
#define KEY_ACTIVE_LEVEL   0

    /* ================== 你在 main.h 里要有这些定义 ==================
     * PREV_Pin, PREV_GPIO_Port
     * NEXT_Pin, NEXT_GPIO_Port
     * MODE_Pin, MODE_GPIO_Port
     *
     * 你说你在 main.h 里定义了引脚，那一般 CubeMX 会生成这些名字。
     * 如果你的名字不是 PREV/NEXT/MODE，而是别的（比如 KEY1），你只要改这里三行宏即可。
     */
#define KEY_READ_PREV()    (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin))
#define KEY_READ_NEXT()    (HAL_GPIO_ReadPin(KEY_2_GPIO_Port, KEY_2_Pin))
#define KEY_READ_MODE()    (HAL_GPIO_ReadPin(KEY_3_GPIO_Port, KEY_3_Pin))

    /* ================== 去抖参数 ==================
     * 10ms 调一次 scan
     * KEY_DEBOUNCE_CNT=3 => 30ms 去抖（很常用）
     */
#ifndef KEY_DEBOUNCE_CNT
#define KEY_DEBOUNCE_CNT   3u
#endif

    typedef enum {
        KEY_PREV = 0,
        KEY_NEXT = 1,
        KEY_MODE = 2,
    } key_id_t;

#define KEY_MASK_PREV   (1u << KEY_PREV)
#define KEY_MASK_NEXT   (1u << KEY_NEXT)
#define KEY_MASK_MODE   (1u << KEY_MODE)

    void     key_init(void);

    /* 放在你的 10ms 节拍里调用 */
    void     key_scan_10ms(void);

    /* 刚按下触发一次（读一次清零） */
    uint8_t  key_trg_get(void);

    /* 当前稳定按住状态（不清零） */
    uint8_t  key_cont_get(void);

#ifdef __cplusplus
}
#endif
