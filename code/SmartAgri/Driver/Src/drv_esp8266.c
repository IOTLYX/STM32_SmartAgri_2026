#include "drv_esp8266.h"
#include <string.h>
#include <stdio.h>

/* ===== 串口句柄 ===== */

/* ESP8266 通信串口句柄 */
static UART_HandleTypeDef *s_esp = NULL;

/* 调试输出串口句柄，可为 NULL */
static UART_HandleTypeDef *s_dbg = NULL;

/* ===== DMA RX 缓冲 ===== */

/**
 * @brief UART DMA 环形接收缓冲区
 *
 * 用于承接串口 DMA 循环接收的数据。
 */
static uint8_t s_rx_dma_buf[ESP8266_RX_DMA_SIZE];

/**
 * @brief 上一次 DMA 已搬运位置
 *
 * 用于比较 DMA 当前写入位置，只搬运新增数据到软件 Ring。
 */
static uint16_t s_rx_dma_last = 0;

/* ===== 软件 Ring ===== */

/**
 * @brief 软件环形缓冲区
 *
 * 用于缓存从 DMA 缓冲区搬运出的串口数据，
 * 供上层按字节读取和响应匹配使用。
 */
static uint8_t s_rx_ring[ESP8266_RX_RING_SIZE];

/**
 * @brief 软件 Ring 写指针
 */
static volatile uint16_t s_rx_w = 0;

/**
 * @brief 软件 Ring 读指针
 */
static volatile uint16_t s_rx_r = 0;

/* ===== 最近一次响应缓存 ===== */

/**
 * @brief 最近一次响应字符串缓存
 *
 * 用于保存本次等待过程中收到的完整响应文本，
 * 便于匹配 expect1 / expect2 或后续调试查看。
 */
static char s_last_resp[ESP8266_TMP_RESP_SIZE];

/* =========================================================
 * DBG 输出（阻塞发送，逻辑简单，调试稳定）
 * ========================================================= */

/**
 * @brief 调试串口发送指定长度数据
 *
 * 采用阻塞发送方式，仅用于调试输出。
 *
 * @param p 待发送数据指针
 * @param n 数据长度
 */
static void _dbg_tx(const uint8_t *p, uint16_t n)
{
    if (!s_dbg || !p || n == 0)
    {
        return;
    }

    (void)HAL_UART_Transmit(s_dbg, (uint8_t *)p, n, 2000);
}

/**
 * @brief 调试串口发送字符串
 *
 * @param s 待发送字符串
 */
static void _dbg_str(const char *s)
{
    if (!s_dbg || !s)
    {
        return;
    }

    _dbg_tx((const uint8_t *)s, (uint16_t)strlen(s));
}

/**
 * @brief 调试串口输出一行文本
 *
 * 一般用于输出前缀 + 内容 + 换行。
 *
 * @param pfx 前缀字符串，可为 NULL
 * @param s   主体字符串，可为 NULL
 */
static void _dbg_line(const char *pfx, const char *s)
{
    if (!s_dbg)
    {
        return;
    }

    if (pfx)
    {
        _dbg_str(pfx);
    }

    if (s)
    {
        _dbg_str(s);
    }

    _dbg_str("\r\n");
}

/* =========================================================
 * Ring 操作
 * ========================================================= */

/**
 * @brief 仅清空软件 Ring 缓冲区
 *
 * 不处理 DMA 当前已接收数据，仅重置软件读写指针。
 */
static void _rx_ring_clear_only(void)
{
    s_rx_w = 0;
    s_rx_r = 0;
}

/**
 * @brief 向软件 Ring 写入 1 字节
 *
 * 若 Ring 已满，则丢弃最旧的 1 字节，
 * 以保证新数据仍然能够写入，避免匹配逻辑长期卡死。
 *
 * @param b 待写入字节
 */
static void _rx_ring_push(uint8_t b)
{
    uint16_t next = (uint16_t)(s_rx_w + 1u);
    if (next >= ESP8266_RX_RING_SIZE)
    {
        next = 0;
    }

    if (next == s_rx_r)
    {
        /* Ring 满：丢弃最旧 1 字节 */
        uint16_t rr = (uint16_t)(s_rx_r + 1u);
        if (rr >= ESP8266_RX_RING_SIZE)
        {
            rr = 0;
        }
        s_rx_r = rr;
    }

    s_rx_ring[s_rx_w] = b;
    s_rx_w = next;
}

/**
 * @brief 向软件 Ring 连续写入一段数据
 *
 * @param p 数据源指针
 * @param n 数据长度
 */
static void _rx_ring_push_buf(const uint8_t *p, uint16_t n)
{
    for (uint16_t i = 0; i < n; i++)
    {
        _rx_ring_push(p[i]);
    }
}

/**
 * @brief 从软件 Ring 读取 1 字节
 *
 * 读取前会先尝试把 DMA 新收到的数据搬运进 Ring。
 *
 * @param out_ch 输出字节指针
 * @return true  成功读到 1 字节
 * @return false 当前无可读数据或参数无效
 */
bool ESP8266_RxPop(uint8_t *out_ch)
{
    if (!out_ch)
    {
        return false;
    }

    /* 先把 DMA 新数据搬进 Ring */
    ESP8266_RxDmaPoll();

    if (s_rx_r == s_rx_w)
    {
        return false;
    }

    *out_ch = s_rx_ring[s_rx_r];

    uint16_t rr = (uint16_t)(s_rx_r + 1u);
    if (rr >= ESP8266_RX_RING_SIZE)
    {
        rr = 0;
    }
    s_rx_r = rr;

    return true;
}

/* =========================================================
 * DMA -> Ring 搬运
 * ========================================================= */

/**
 * @brief 获取 DMA 当前写入位置
 *
 * 计算方式：
 * pos = DMA 总长度 - DMA 剩余计数值
 *
 * @return 当前 DMA 写入位置
 */
static uint16_t _dma_pos_now(void)
{
    if (!s_esp || !s_esp->hdmarx)
    {
        return 0;
    }

    uint16_t remain = (uint16_t)__HAL_DMA_GET_COUNTER(s_esp->hdmarx);

    if (remain > ESP8266_RX_DMA_SIZE)
    {
        remain = ESP8266_RX_DMA_SIZE;
    }

    return (uint16_t)(ESP8266_RX_DMA_SIZE - remain);
}

/**
 * @brief 将 DMA 新接收的数据搬运到软件 Ring
 *
 * 支持 DMA 环形模式回卷处理。
 */
void ESP8266_RxDmaPoll(void)
{
    if (!s_esp || !s_esp->hdmarx)
    {
        return;
    }

    uint16_t pos = _dma_pos_now();
    if (pos == s_rx_dma_last)
    {
        return;
    }

    if (pos > s_rx_dma_last)
    {
        /* DMA 未回卷，直接搬运新增区间 */
        _rx_ring_push_buf(&s_rx_dma_buf[s_rx_dma_last],
                          (uint16_t)(pos - s_rx_dma_last));
    }
    else
    {
        /* DMA 已回卷，先搬运尾段，再搬运起始段 */
        _rx_ring_push_buf(&s_rx_dma_buf[s_rx_dma_last],
                          (uint16_t)(ESP8266_RX_DMA_SIZE - s_rx_dma_last));

        if (pos > 0)
        {
            _rx_ring_push_buf(&s_rx_dma_buf[0], pos);
        }
    }

    s_rx_dma_last = pos;
}

/**
 * @brief 清空接收缓存
 *
 * 清空软件 Ring，并同步丢弃 DMA 当前已经接收但尚未搬运的数据，
 * 用于避免历史残留响应影响下一条 AT 命令匹配。
 */
void ESP8266_RxClear(void)
{
    _rx_ring_clear_only();

    /* 丢弃 DMA 当前点之前已经接收到的数据 */
    s_rx_dma_last = _dma_pos_now();
}

/* =========================================================
 * 发送
 * ========================================================= */

/**
 * @brief 发送以 '\0' 结尾的字符串
 *
 * 采用阻塞发送方式。
 *
 * @param s 待发送字符串
 * @return HAL 串口发送状态
 */
HAL_StatusTypeDef ESP8266_SendRawStr(const char *s)
{
    if (!s_esp || !s)
    {
        return HAL_ERROR;
    }

    return HAL_UART_Transmit(s_esp, (uint8_t *)s, (uint16_t)strlen(s), 2000);
}

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
HAL_StatusTypeDef ESP8266_SendRawLen(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (!s_esp || !data || len == 0)
    {
        return HAL_ERROR;
    }

    return HAL_UART_Transmit(s_esp, (uint8_t *)data, len, timeout_ms);
}

/* =========================================================
 * Init
 * ========================================================= */

/**
 * @brief 初始化 ESP8266 驱动
 *
 * 保存串口句柄，清空接收缓存，并启动 UART DMA 循环接收。
 * 若提供调试串口，则输出初始化日志。
 *
 * @param huart_esp ESP8266 通信串口句柄
 * @param huart_dbg 调试输出串口句柄，可为 NULL
 */
void ESP8266_Init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_dbg)
{
    s_esp = huart_esp;
    s_dbg = huart_dbg;

    ESP8266_RxClear();
    memset(s_last_resp, 0, sizeof(s_last_resp));

    if (!s_esp)
    {
        return;
    }

    /* 启动 UART RX DMA，要求 CubeMX 中已配置为 Circular 模式 */
    (void)HAL_UART_Receive_DMA(s_esp, s_rx_dma_buf, ESP8266_RX_DMA_SIZE);

    /* 关闭 DMA 半传输/全传输中断，减少无意义中断回调 */
    if (s_esp->hdmarx)
    {
        __HAL_DMA_DISABLE_IT(s_esp->hdmarx, DMA_IT_HT);
        __HAL_DMA_DISABLE_IT(s_esp->hdmarx, DMA_IT_TC);
    }

    if (s_dbg)
    {
        _dbg_line("[ESP8266] ", "init (DMA RX + ring)");
    }
}

/**
 * @brief 获取最近一次响应缓存
 *
 * @return 最近一次响应字符串指针
 */
const char *ESP8266_GetLastResp(void)
{
    return s_last_resp;
}

/* =========================================================
 * 等待与匹配
 * ========================================================= */

/**
 * @brief 判断缓冲区中是否命中目标字符串
 *
 * @param buf 待搜索字符串
 * @param pat 目标模式串
 * @return true  命中
 * @return false 未命中
 */
static bool _hit(const char *buf, const char *pat)
{
    if (!pat || !pat[0])
    {
        return false;
    }

    return (strstr(buf, pat) != NULL);
}

/**
 * @brief 判断当前响应中是否包含明确错误信息
 *
 * 仅将 ERROR / FAIL / CLOSED 视为失败。
 * 不把 WIFI DISCONNECT / +MQTTDISCONNECTED 之类异步提示算作错误，
 * 以避免误伤正常状态切换日志。
 *
 * @param buf 当前响应缓存
 * @return true  检测到错误文本
 * @return false 未检测到错误文本
 */
static bool _is_error_text(const char *buf)
{
    if (!buf || !buf[0])
    {
        return false;
    }

    if (strstr(buf, "\r\nERROR\r\n"))
    {
        return true;
    }
    if (strstr(buf, "\r\nFAIL\r\n"))
    {
        return true;
    }
    if (strstr(buf, "ERROR"))
    {
        return true;
    }
    if (strstr(buf, "FAIL"))
    {
        return true;
    }
    if (strstr(buf, "CLOSED"))
    {
        return true;
    }

    return false;
}

/**
 * @brief 按需将接收到的字符回显到调试串口
 *
 * 为减少串口发送次数，采用小缓冲累积后批量输出。
 *
 * @param echo_to_dbg 是否启用回显
 * @param ch          当前收到的字节
 * @param echo_buf    回显缓冲区
 * @param echo_n      当前回显缓冲长度
 */
static void _maybe_echo(bool echo_to_dbg, uint8_t ch, uint8_t *echo_buf, uint16_t *echo_n)
{
    if (!echo_to_dbg || !s_dbg)
    {
        return;
    }

    echo_buf[*echo_n] = ch;
    (*echo_n)++;

    if (*echo_n >= 64u)
    {
        _dbg_tx(echo_buf, *echo_n);
        *echo_n = 0;
    }
}

/**
 * @brief 发送 AT 命令并等待响应
 *
 * 执行流程：
 * 1. 清空接收缓存，避免历史残留干扰
 * 2. 发送 AT 命令
 * 3. 持续接收并匹配 expect1 / expect2
 * 4. 若命中错误文本，则返回失败
 * 5. 超时则返回超时错误
 *
 * @param cmd         待发送命令字符串
 * @param expect1     期望响应 1
 * @param expect2     期望响应 2，可为 NULL
 * @param timeout_ms  等待超时时间，单位 ms
 * @param echo_to_dbg 是否将接收内容回显到调试串口
 * @return 执行结果
 */
esp8266_res_t ESP8266_SendCmdWait(const char *cmd,
                                  const char *expect1,
                                  const char *expect2,
                                  uint32_t timeout_ms,
                                  bool echo_to_dbg)
{
    if (!s_esp || !cmd || cmd[0] == '\0')
    {
        return ESP8266_RES_PARAM;
    }

    /* 每条命令前清空 Ring，避免残留响应影响匹配 */
    ESP8266_RxClear();
    memset(s_last_resp, 0, sizeof(s_last_resp));

    if (s_dbg)
    {
        _dbg_str("TX> ");
        _dbg_str(cmd);
    }

    if (ESP8266_SendRawStr(cmd) != HAL_OK)
    {
        if (s_dbg)
        {
            _dbg_line("RES> ", "TX FAIL");
        }
        return ESP8266_RES_UART;
    }

    uint32_t t0 = HAL_GetTick();
    uint16_t n = 0;

    uint8_t echo_buf[64];
    uint16_t echo_n = 0;

    while ((uint32_t)(HAL_GetTick() - t0) < timeout_ms)
    {
        uint8_t ch;
        while (ESP8266_RxPop(&ch))
        {
            if (n < (ESP8266_TMP_RESP_SIZE - 1u))
            {
                s_last_resp[n++] = (char)ch;
                s_last_resp[n] = '\0';
            }
            else
            {
                if (s_dbg)
                {
                    _dbg_line("RES> ", "RESP OVERFLOW");
                }
                return ESP8266_RES_OVERFLOW;
            }

            _maybe_echo(echo_to_dbg, ch, echo_buf, &echo_n);

            if (_hit(s_last_resp, expect1))
            {
                if (echo_to_dbg && s_dbg && echo_n)
                {
                    _dbg_tx(echo_buf, echo_n);
                    echo_n = 0;
                }

                if (s_dbg)
                {
                    _dbg_line("RES> ", "OK (hit expect1)");
                }
                return ESP8266_RES_OK;
            }

            if (_hit(s_last_resp, expect2))
            {
                if (echo_to_dbg && s_dbg && echo_n)
                {
                    _dbg_tx(echo_buf, echo_n);
                    echo_n = 0;
                }

                if (s_dbg)
                {
                    _dbg_line("RES> ", "OK (hit expect2)");
                }
                return ESP8266_RES_OK;
            }

            if (_is_error_text(s_last_resp))
            {
                if (echo_to_dbg && s_dbg && echo_n)
                {
                    _dbg_tx(echo_buf, echo_n);
                    echo_n = 0;
                }

                if (s_dbg)
                {
                    _dbg_line("RES> ", "ERROR/FAIL/CLOSED");
                }
                return ESP8266_RES_ERROR;
            }
        }

        /* 小延时让步，避免 while 空转吃满 CPU */
        HAL_Delay(1);
    }

    if (echo_to_dbg && s_dbg && echo_n)
    {
        _dbg_tx(echo_buf, echo_n);
        echo_n = 0;
    }

    if (s_dbg)
    {
        _dbg_line("RES> ", "TIMEOUT");
    }

    return ESP8266_RES_TIMEOUT;
}

/**
 * @brief 不发送命令，仅等待响应
 *
 * 常用于等待模组异步上报或某条命令的后续返回内容。
 *
 * @param expect1     期望响应 1
 * @param expect2     期望响应 2，可为 NULL
 * @param timeout_ms  等待超时时间，单位 ms
 * @param echo_to_dbg 是否将接收内容回显到调试串口
 * @return 执行结果
 */
esp8266_res_t ESP8266_WaitResp(const char *expect1,
                               const char *expect2,
                               uint32_t timeout_ms,
                               bool echo_to_dbg)
{
    if (!s_esp)
    {
        return ESP8266_RES_PARAM;
    }

    memset(s_last_resp, 0, sizeof(s_last_resp));

    uint32_t t0 = HAL_GetTick();
    uint16_t n = 0;

    uint8_t echo_buf[64];
    uint16_t echo_n = 0;

    while ((uint32_t)(HAL_GetTick() - t0) < timeout_ms)
    {
        uint8_t ch;
        while (ESP8266_RxPop(&ch))
        {
            if (n < (ESP8266_TMP_RESP_SIZE - 1u))
            {
                s_last_resp[n++] = (char)ch;
                s_last_resp[n] = '\0';
            }
            else
            {
                if (s_dbg)
                {
                    _dbg_line("RES> ", "RESP OVERFLOW");
                }
                return ESP8266_RES_OVERFLOW;
            }

            _maybe_echo(echo_to_dbg, ch, echo_buf, &echo_n);

            if (_hit(s_last_resp, expect1))
            {
                if (echo_to_dbg && s_dbg && echo_n)
                {
                    _dbg_tx(echo_buf, echo_n);
                    echo_n = 0;
                }

                if (s_dbg)
                {
                    _dbg_line("RES> ", "OK (wait hit expect1)");
                }
                return ESP8266_RES_OK;
            }

            if (_hit(s_last_resp, expect2))
            {
                if (echo_to_dbg && s_dbg && echo_n)
                {
                    _dbg_tx(echo_buf, echo_n);
                    echo_n = 0;
                }

                if (s_dbg)
                {
                    _dbg_line("RES> ", "OK (wait hit expect2)");
                }
                return ESP8266_RES_OK;
            }

            if (_is_error_text(s_last_resp))
            {
                if (echo_to_dbg && s_dbg && echo_n)
                {
                    _dbg_tx(echo_buf, echo_n);
                    echo_n = 0;
                }

                if (s_dbg)
                {
                    _dbg_line("RES> ", "ERROR/FAIL/CLOSED");
                }
                return ESP8266_RES_ERROR;
            }
        }

        /* 小延时让步，避免 while 空转吃满 CPU */
        HAL_Delay(1);
    }

    if (echo_to_dbg && s_dbg && echo_n)
    {
        _dbg_tx(echo_buf, echo_n);
        echo_n = 0;
    }

    if (s_dbg)
    {
        _dbg_line("RES> ", "TIMEOUT");
    }

    return ESP8266_RES_TIMEOUT;
}