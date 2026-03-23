#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ESP8266 DMA 接收环形缓冲区大小
 *
 * 用于 UART DMA 循环接收缓存。
 */
#ifndef ESP8266_RX_DMA_SIZE
#define ESP8266_RX_DMA_SIZE      1024u
#endif

/**
 * @brief ESP8266 软件环形缓冲区大小
 *
 * DMA 搬运后的数据将进入该软件 ring buffer。
 */
#ifndef ESP8266_RX_RING_SIZE
#define ESP8266_RX_RING_SIZE     2048u
#endif

/**
 * @brief 最近一次响应临时缓存区大小
 *
 * 用于保存 last_resp，便于调试查看模组返回内容。
 */
#ifndef ESP8266_TMP_RESP_SIZE
#define ESP8266_TMP_RESP_SIZE    1024u
#endif

/**
 * @brief ESP8266 操作结果码
 */
typedef enum
{
    ESP8266_RES_OK = 0,       /**< 操作成功 */
    ESP8266_RES_ERROR = 1,    /**< 收到错误响应 */
    ESP8266_RES_TIMEOUT = 2,  /**< 等待超时 */
    ESP8266_RES_OVERFLOW = 3, /**< 缓冲区溢出 */
    ESP8266_RES_UART = 4,     /**< 串口底层错误 */
    ESP8266_RES_PARAM = 5,    /**< 参数错误 */
} esp8266_res_t;

/**
 * @brief 初始化 ESP8266 驱动
 *
 * 绑定 ESP8266 通信串口和调试输出串口。
 * 调试串口可传入 NULL，表示不输出调试信息。
 *
 * @param huart_esp ESP8266 通信串口句柄
 * @param huart_dbg 调试串口句柄，可为 NULL
 */
void ESP8266_Init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_dbg);

/**
 * @brief 清空接收缓冲区
 *
 * 清空软件 ring buffer，并同步丢弃 DMA 当前已接收但尚未处理的数据，
 * 用于避免旧响应残留影响后续 AT 命令解析。
 */
void ESP8266_RxClear(void);

/**
 * @brief 从软件 ring buffer 取出一个字节
 *
 * @param out_ch 输出字节指针
 * @return true  成功取到 1 字节
 * @return false 当前无可读数据
 */
bool ESP8266_RxPop(uint8_t *out_ch);

/**
 * @brief 将 DMA 新接收的数据搬运到软件 ring buffer
 *
 * 一般无需手动调用，内部在 RxPop/Wait 过程中会自动调用。
 * 若有特殊轮询需求，也可主动调用该接口。
 */
void ESP8266_RxDmaPoll(void);

/**
 * @brief 发送字符串数据
 *
 * 采用阻塞发送方式，字符串长度由 '\0' 结束符决定。
 *
 * @param s 待发送字符串
 * @return HAL 串口发送状态
 */
HAL_StatusTypeDef ESP8266_SendRawStr(const char *s);

/**
 * @brief 发送指定长度的原始数据
 *
 * 采用阻塞发送方式。
 *
 * @param data       待发送数据指针
 * @param len        数据长度
 * @param timeout_ms 发送超时时间，单位 ms
 * @return HAL 串口发送状态
 */
HAL_StatusTypeDef ESP8266_SendRawLen(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief 发送 AT 命令并等待响应
 *
 * 发送命令后等待 expect1 或 expect2 命中。
 * 其中 expect2 可传 NULL，表示只匹配一个目标响应。
 *
 * @param cmd         AT 命令字符串
 * @param expect1     期望匹配字符串 1
 * @param expect2     期望匹配字符串 2，可为 NULL
 * @param timeout_ms  等待超时时间，单位 ms
 * @param echo_to_dbg 是否同步输出响应到调试串口
 * @return 执行结果，详见 @ref esp8266_res_t
 */
esp8266_res_t ESP8266_SendCmdWait(const char *cmd,
                                  const char *expect1,
                                  const char *expect2,
                                  uint32_t timeout_ms,
                                  bool echo_to_dbg);

/**
 * @brief 不发送命令，仅等待指定响应
 *
 * 常用于等待模组异步返回或上一次命令的后续结果。
 *
 * @param expect1     期望匹配字符串 1
 * @param expect2     期望匹配字符串 2，可为 NULL
 * @param timeout_ms  等待超时时间，单位 ms
 * @param echo_to_dbg 是否同步输出响应到调试串口
 * @return 执行结果，详见 @ref esp8266_res_t
 */
esp8266_res_t ESP8266_WaitResp(const char *expect1,
                               const char *expect2,
                               uint32_t timeout_ms,
                               bool echo_to_dbg);

/**
 * @brief 获取最近一次响应缓存
 *
 * 常用于调试打印或错误定位。
 *
 * @return 最近一次响应字符串指针
 */
const char *ESP8266_GetLastResp(void);

#ifdef __cplusplus
}
#endif