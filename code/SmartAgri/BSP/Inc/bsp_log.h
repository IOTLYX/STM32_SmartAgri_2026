#pragma once
#include <stdarg.h>
#include <stdint.h>
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

    /* 是否打印毫秒时间戳（1=开，0=关） */
#ifndef BSP_LOG_WITH_TIMESTAMP
#define BSP_LOG_WITH_TIMESTAMP  1
#endif

    /* 单条日志最大长度*/
#ifndef BSP_LOG_BUF_SIZE
#define BSP_LOG_BUF_SIZE        256
#endif

    /* 发送超时（ms） */
#ifndef BSP_LOG_TX_TIMEOUT_MS
#define BSP_LOG_TX_TIMEOUT_MS   50
#endif

    /* 初始化 */
    void bsp_log_init(UART_HandleTypeDef *huart);

    /* 打印一行 */
    void bsp_log_printf(const char *fmt, ...);

    /* 带等级前缀的日志 */
    void bsp_log_print_level(char level, const char *fmt, ...);

    /* 便捷宏 */
#define LOGI(...)  bsp_log_print_level('I', __VA_ARGS__)
#define LOGW(...)  bsp_log_print_level('W', __VA_ARGS__)
#define LOGE(...)  bsp_log_print_level('E', __VA_ARGS__)

#ifdef __cplusplus
}
#endif
