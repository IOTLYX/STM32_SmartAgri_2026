#pragma once

#include "stm32f1xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 按键编号定义
 *
 * 共 3 个按键，按下为 1，未按下为 0（Active High）。
 */
typedef enum
{
    KEY_PREV = 0,  /* 上一项/确认类按键 */
    KEY_NEXT = 1,  /* 下一项按键 */
    KEY_MODE = 2,  /* 模式切换按键 */
} bsp_key_t;

/* 按键触发位掩码 */
#define KEY_MASK_PREV   (1u << KEY_PREV)
#define KEY_MASK_NEXT   (1u << KEY_NEXT)
#define KEY_MASK_MODE   (1u << KEY_MODE)

/**
 * @brief 按键消抖计数阈值
 *
 * 按 10ms 扫描一次时，连续 3 次一致即认为状态稳定，
 * 对应约 30ms 消抖时间。
 */
#ifndef BSP_KEY_DEBOUNCE_CNT
#define BSP_KEY_DEBOUNCE_CNT  3u
#endif

/**
 * @brief 初始化按键驱动
 *
 * 完成按键 GPIO 状态与内部扫描状态机初始化。
 */
void bsp_key_init(void);

/**
 * @brief 按键 10ms 扫描函数
 *
 * 需要放在固定 10ms 周期中调用，
 * 用于按键消抖、边沿检测和稳定态更新。
 */
void bsp_key_scan_10ms(void);

/**
 * @brief 获取按键触发标志
 *
 * 返回“刚按下”的触发事件，读取一次后清零。
 *
 * @return uint8_t 按键触发位掩码
 */
uint8_t bsp_key_trg_get(void);

/**
 * @brief 获取当前稳定按下状态
 *
 * 返回当前已消抖后的稳定按下状态，不清零。
 *
 * @return uint8_t 当前按键稳定状态位掩码
 */
uint8_t bsp_key_cont_get(void);

#ifdef __cplusplus
}
#endif