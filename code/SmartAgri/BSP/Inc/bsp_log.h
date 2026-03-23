#pragma once

#include <stdarg.h>
#include <stdint.h>
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 是否在日志前输出时间戳
 *
 * 1：输出时间戳
 * 0：不输出时间戳
 */
#ifndef BSP_LOG_WITH_TIMESTAMP
#define BSP_LOG_WITH_TIMESTAMP  1
#endif

/**
 * @brief 单条日志缓冲区大小
 */
#ifndef BSP_LOG_BUF_SIZE
#define BSP_LOG_BUF_SIZE        256
#endif

/**
 * @brief 日志串口发送超时时间，单位 ms
 */
#ifndef BSP_LOG_TX_TIMEOUT_MS
#define BSP_LOG_TX_TIMEOUT_MS   50
#endif

/**
 * @brief 初始化日志模块
 *
 * 绑定底层串口句柄，供后续日志输出使用。
 *
 * @param huart 日志输出串口句柄
 */
void bsp_log_init(UART_HandleTypeDef *huart);

/**
 * @brief 打印普通格式化日志
 *
 * 自动按配置决定是否携带时间戳。
 *
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void bsp_log_printf(const char *fmt, ...);

/**
 * @brief 打印带等级前缀的日志
 *
 * 常用于输出 I/W/E 等等级标识。
 *
 * @param level 日志等级字符，如 'I'、'W'、'E'
 * @param fmt   格式化字符串
 * @param ...   可变参数
 */
void bsp_log_print_level(char level, const char *fmt, ...);

/* 日志便捷宏 */
#define LOGI(...)  bsp_log_print_level('I', __VA_ARGS__)
#define LOGW(...)  bsp_log_print_level('W', __VA_ARGS__)
#define LOGE(...)  bsp_log_print_level('E', __VA_ARGS__)

#ifdef __cplusplus
}
#endif