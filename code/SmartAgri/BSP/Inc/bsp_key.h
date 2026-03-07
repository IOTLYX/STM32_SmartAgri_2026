#pragma once
#include "stm32f1xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    /* 3个键：不按=0，按下=1（Active High） */
    typedef enum {
        KEY_PREV = 0,
        KEY_NEXT = 1,
        KEY_MODE = 2,
    } bsp_key_t;

#define KEY_MASK_PREV   (1u << KEY_PREV)
#define KEY_MASK_NEXT   (1u << KEY_NEXT)
#define KEY_MASK_MODE   (1u << KEY_MODE)

    /* 去抖：10ms 调一次，3次一致=30ms */
#ifndef BSP_KEY_DEBOUNCE_CNT
#define BSP_KEY_DEBOUNCE_CNT  3u
#endif

    void    bsp_key_init(void);

    /* 放到你的 10ms 节拍里调用 */
    void    bsp_key_scan_10ms(void);

    /* 刚按下触发一次（读一次清零） */
    uint8_t bsp_key_trg_get(void);

    /* 当前稳定按住状态（不清零） */
    uint8_t bsp_key_cont_get(void);

#ifdef __cplusplus
}
#endif
