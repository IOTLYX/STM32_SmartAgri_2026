#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

    /* ===== 可调参数 ===== */
#ifndef ESP8266_RX_DMA_SIZE
#define ESP8266_RX_DMA_SIZE      1024u     /* DMA 环形缓冲 */
#endif

#ifndef ESP8266_RX_RING_SIZE
#define ESP8266_RX_RING_SIZE     2048u     /* 软件 ring */
#endif

#ifndef ESP8266_TMP_RESP_SIZE
#define ESP8266_TMP_RESP_SIZE    1024u     /* last_resp 缓冲 */
#endif

    typedef enum {
        ESP8266_RES_OK = 0,
        ESP8266_RES_ERROR = 1,
        ESP8266_RES_TIMEOUT = 2,
        ESP8266_RES_OVERFLOW = 3,
        ESP8266_RES_UART = 4,
        ESP8266_RES_PARAM = 5,
    } esp8266_res_t;

    /* 初始化：ESP 串口 + DBG 串口（DBG 可传 NULL） */
    void ESP8266_Init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_dbg);

    /* 清空软件 ring，并同步丢弃 DMA 当前已接收的数据（防残留） */
    void ESP8266_RxClear(void);

    /* 从 ring 取 1 字节（有则返回 true） */
    bool ESP8266_RxPop(uint8_t *out_ch);

    /* 主动从 DMA 搬运新增字节到 ring（一般不必手动调用，RxPop/Wait 内部会自动调用） */
    void ESP8266_RxDmaPoll(void);

    /* 发送：字符串（阻塞） */
    HAL_StatusTypeDef ESP8266_SendRawStr(const char *s);

    /* 发送：指定长度（阻塞） */
    HAL_StatusTypeDef ESP8266_SendRawLen(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

    /* 发送命令并等待 expect1/expect2 命中（expect2 可为 NULL） */
    esp8266_res_t ESP8266_SendCmdWait(const char *cmd,
                                     const char *expect1,
                                     const char *expect2,
                                     uint32_t timeout_ms,
                                     bool echo_to_dbg);

    /* 不发送，只等待 expect1/expect2 命中 */
    esp8266_res_t ESP8266_WaitResp(const char *expect1,
                                  const char *expect2,
                                  uint32_t timeout_ms,
                                  bool echo_to_dbg);

    /* 取最近一次响应缓存（调试用） */
    const char *ESP8266_GetLastResp(void);

#ifdef __cplusplus
}
#endif
