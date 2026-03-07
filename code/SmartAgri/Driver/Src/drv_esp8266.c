#include "drv_esp8266.h"
#include <string.h>
#include <stdio.h>

/* ===== 句柄 ===== */
static UART_HandleTypeDef *s_esp = NULL;
static UART_HandleTypeDef *s_dbg = NULL;

/* ===== DMA RX 缓冲 ===== */
static uint8_t  s_rx_dma_buf[ESP8266_RX_DMA_SIZE];
static uint16_t s_rx_dma_last = 0;

/* ===== 软件 Ring ===== */
static uint8_t  s_rx_ring[ESP8266_RX_RING_SIZE];
static volatile uint16_t s_rx_w = 0;
static volatile uint16_t s_rx_r = 0;

/* ===== last_resp ===== */
static char s_last_resp[ESP8266_TMP_RESP_SIZE];

/* =========================================================
 * DBG 输出（阻塞，简单稳定）
 * ========================================================= */
static void _dbg_tx(const uint8_t *p, uint16_t n)
{
    if (!s_dbg || !p || n == 0) return;
    (void)HAL_UART_Transmit(s_dbg, (uint8_t*)p, n, 2000);
}
static void _dbg_str(const char *s)
{
    if (!s_dbg || !s) return;
    _dbg_tx((const uint8_t*)s, (uint16_t)strlen(s));
}
static void _dbg_line(const char *pfx, const char *s)
{
    if (!s_dbg) return;
    if (pfx) _dbg_str(pfx);
    if (s)   _dbg_str(s);
    _dbg_str("\r\n");
}

/* =========================================================
 * Ring 操作
 * ========================================================= */
static void _rx_ring_clear_only(void)
{
    s_rx_w = 0;
    s_rx_r = 0;
}

static void _rx_ring_push(uint8_t b)
{
    uint16_t next = (uint16_t)(s_rx_w + 1u);
    if (next >= ESP8266_RX_RING_SIZE) next = 0;

    if (next == s_rx_r) {
        /* 满了：丢弃最旧 1 字节（更“抗抖”，比直接不写更不容易卡死匹配） */
        uint16_t rr = (uint16_t)(s_rx_r + 1u);
        if (rr >= ESP8266_RX_RING_SIZE) rr = 0;
        s_rx_r = rr;
    }

    s_rx_ring[s_rx_w] = b;
    s_rx_w = next;
}

static void _rx_ring_push_buf(const uint8_t *p, uint16_t n)
{
    for (uint16_t i = 0; i < n; i++) _rx_ring_push(p[i]);
}

bool ESP8266_RxPop(uint8_t *out_ch)
{
    if (!out_ch) return false;

    /* 先把 DMA 新数据搬进 ring */
    ESP8266_RxDmaPoll();

    if (s_rx_r == s_rx_w) return false;

    *out_ch = s_rx_ring[s_rx_r];

    uint16_t rr = (uint16_t)(s_rx_r + 1u);
    if (rr >= ESP8266_RX_RING_SIZE) rr = 0;
    s_rx_r = rr;
    return true;
}

/* =========================================================
 * DMA -> Ring 搬运
 * ========================================================= */
static uint16_t _dma_pos_now(void)
{
    if (!s_esp || !s_esp->hdmarx) return 0;

    /* pos = size - remaining */
    uint16_t remain = (uint16_t)__HAL_DMA_GET_COUNTER(s_esp->hdmarx);
    if (remain > ESP8266_RX_DMA_SIZE) remain = ESP8266_RX_DMA_SIZE;
    return (uint16_t)(ESP8266_RX_DMA_SIZE - remain);
}

void ESP8266_RxDmaPoll(void)
{
    if (!s_esp || !s_esp->hdmarx) return;

    uint16_t pos = _dma_pos_now();
    if (pos == s_rx_dma_last) return;

    if (pos > s_rx_dma_last) {
        _rx_ring_push_buf(&s_rx_dma_buf[s_rx_dma_last], (uint16_t)(pos - s_rx_dma_last));
    } else {
        /* 回卷 */
        _rx_ring_push_buf(&s_rx_dma_buf[s_rx_dma_last], (uint16_t)(ESP8266_RX_DMA_SIZE - s_rx_dma_last));
        if (pos > 0) _rx_ring_push_buf(&s_rx_dma_buf[0], pos);
    }
    s_rx_dma_last = pos;
}

void ESP8266_RxClear(void)
{
    _rx_ring_clear_only();

    /* 同步：把 DMA 当前点设为 last，等价于丢弃“之前已接收但未搬运”的字节 */
    s_rx_dma_last = _dma_pos_now();
}

/* =========================================================
 * 发送
 * ========================================================= */
HAL_StatusTypeDef ESP8266_SendRawStr(const char *s)
{
    if (!s_esp || !s) return HAL_ERROR;
    return HAL_UART_Transmit(s_esp, (uint8_t*)s, (uint16_t)strlen(s), 2000);
}

HAL_StatusTypeDef ESP8266_SendRawLen(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (!s_esp || !data || len == 0) return HAL_ERROR;
    return HAL_UART_Transmit(s_esp, (uint8_t*)data, len, timeout_ms);
}

/* =========================================================
 * Init
 * ========================================================= */
void ESP8266_Init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_dbg)
{
    s_esp = huart_esp;
    s_dbg = huart_dbg;

    ESP8266_RxClear();
    memset(s_last_resp, 0, sizeof(s_last_resp));

    if (!s_esp) return;

    /* 启动 RX DMA（Circular 模式要在 CubeMX 配好） */
    (void)HAL_UART_Receive_DMA(s_esp, s_rx_dma_buf, ESP8266_RX_DMA_SIZE);

    /* 关掉 DMA 半传/全传中断（可选：减少回调噪音） */
    if (s_esp->hdmarx) {
        __HAL_DMA_DISABLE_IT(s_esp->hdmarx, DMA_IT_HT);
        __HAL_DMA_DISABLE_IT(s_esp->hdmarx, DMA_IT_TC);
    }

    if (s_dbg) _dbg_line("[ESP8266] ", "init (DMA RX + ring)");
}

const char *ESP8266_GetLastResp(void)
{
    return s_last_resp;
}

/* =========================================================
 * 等待与匹配
 * ========================================================= */
static bool _hit(const char *buf, const char *pat)
{
    if (!pat || !pat[0]) return false;
    return (strstr(buf, pat) != NULL);
}

/* 只把“明确错误”当错误：ERROR/FAIL/CLOSED
   不把 WIFI DISCONNECT / +MQTTDISCONNECTED 当成 ERROR（否则会误伤正常异步提示） */
static bool _is_error_text(const char *buf)
{
    if (!buf || !buf[0]) return false;

    if (strstr(buf, "\r\nERROR\r\n")) return true;
    if (strstr(buf, "\r\nFAIL\r\n"))  return true;
    if (strstr(buf, "ERROR"))         return true;
    if (strstr(buf, "FAIL"))          return true;
    if (strstr(buf, "CLOSED"))        return true;  /* 你日志里常见 ERROR/FAIL/CLOSED */
    return false;
}

static void _maybe_echo(bool echo_to_dbg, uint8_t ch, uint8_t *echo_buf, uint16_t *echo_n)
{
    if (!echo_to_dbg || !s_dbg) return;

    echo_buf[*echo_n] = ch;
    (*echo_n)++;

    if (*echo_n >= 64u) {
        _dbg_tx(echo_buf, *echo_n);
        *echo_n = 0;
    }
}

esp8266_res_t ESP8266_SendCmdWait(const char *cmd,
                                 const char *expect1,
                                 const char *expect2,
                                 uint32_t timeout_ms,
                                 bool echo_to_dbg)
{
    if (!s_esp || !cmd || cmd[0] == '\0') return ESP8266_RES_PARAM;

    /* 建议：每条命令前清 ring（避免残留影响匹配） */
    ESP8266_RxClear();
    memset(s_last_resp, 0, sizeof(s_last_resp));

    if (s_dbg) {
        _dbg_str("TX> ");
        _dbg_str(cmd);
    }

    if (ESP8266_SendRawStr(cmd) != HAL_OK) {
        if (s_dbg) _dbg_line("RES> ", "TX FAIL");
        return ESP8266_RES_UART;
    }

    uint32_t t0 = HAL_GetTick();
    uint16_t n = 0;

    uint8_t echo_buf[64];
    uint16_t echo_n = 0;

    while ((uint32_t)(HAL_GetTick() - t0) < timeout_ms) {

        uint8_t ch;
        while (ESP8266_RxPop(&ch)) {

            if (n < (ESP8266_TMP_RESP_SIZE - 1u)) {
                s_last_resp[n++] = (char)ch;
                s_last_resp[n] = '\0';
            } else {
                if (s_dbg) _dbg_line("RES> ", "RESP OVERFLOW");
                return ESP8266_RES_OVERFLOW;
            }

            _maybe_echo(echo_to_dbg, ch, echo_buf, &echo_n);

            if (_hit(s_last_resp, expect1)) {
                if (echo_to_dbg && s_dbg && echo_n) { _dbg_tx(echo_buf, echo_n); echo_n = 0; }
                if (s_dbg) _dbg_line("RES> ", "OK (hit expect1)");
                return ESP8266_RES_OK;
            }
            if (_hit(s_last_resp, expect2)) {
                if (echo_to_dbg && s_dbg && echo_n) { _dbg_tx(echo_buf, echo_n); echo_n = 0; }
                if (s_dbg) _dbg_line("RES> ", "OK (hit expect2)");
                return ESP8266_RES_OK;
            }

            if (_is_error_text(s_last_resp)) {
                if (echo_to_dbg && s_dbg && echo_n) { _dbg_tx(echo_buf, echo_n); echo_n = 0; }
                if (s_dbg) _dbg_line("RES> ", "ERROR/FAIL/CLOSED");
                return ESP8266_RES_ERROR;
            }
        }

        /* 小让步，避免空转吃满CPU */
        HAL_Delay(1);
    }

    if (echo_to_dbg && s_dbg && echo_n) { _dbg_tx(echo_buf, echo_n); echo_n = 0; }
    if (s_dbg) _dbg_line("RES> ", "TIMEOUT");
    return ESP8266_RES_TIMEOUT;
}

esp8266_res_t ESP8266_WaitResp(const char *expect1,
                              const char *expect2,
                              uint32_t timeout_ms,
                              bool echo_to_dbg)
{
    if (!s_esp) return ESP8266_RES_PARAM;

    memset(s_last_resp, 0, sizeof(s_last_resp));

    uint32_t t0 = HAL_GetTick();
    uint16_t n = 0;

    uint8_t echo_buf[64];
    uint16_t echo_n = 0;

    while ((uint32_t)(HAL_GetTick() - t0) < timeout_ms) {

        uint8_t ch;
        while (ESP8266_RxPop(&ch)) {

            if (n < (ESP8266_TMP_RESP_SIZE - 1u)) {
                s_last_resp[n++] = (char)ch;
                s_last_resp[n] = '\0';
            } else {
                if (s_dbg) _dbg_line("RES> ", "RESP OVERFLOW");
                return ESP8266_RES_OVERFLOW;
            }

            _maybe_echo(echo_to_dbg, ch, echo_buf, &echo_n);

            if (_hit(s_last_resp, expect1)) {
                if (echo_to_dbg && s_dbg && echo_n) { _dbg_tx(echo_buf, echo_n); echo_n = 0; }
                if (s_dbg) _dbg_line("RES> ", "OK (wait hit expect1)");
                return ESP8266_RES_OK;
            }
            if (_hit(s_last_resp, expect2)) {
                if (echo_to_dbg && s_dbg && echo_n) { _dbg_tx(echo_buf, echo_n); echo_n = 0; }
                if (s_dbg) _dbg_line("RES> ", "OK (wait hit expect2)");
                return ESP8266_RES_OK;
            }

            if (_is_error_text(s_last_resp)) {
                if (echo_to_dbg && s_dbg && echo_n) { _dbg_tx(echo_buf, echo_n); echo_n = 0; }
                if (s_dbg) _dbg_line("RES> ", "ERROR/FAIL/CLOSED");
                return ESP8266_RES_ERROR;
            }
        }

        HAL_Delay(1);
    }

    if (echo_to_dbg && s_dbg && echo_n) { _dbg_tx(echo_buf, echo_n); echo_n = 0; }
    if (s_dbg) _dbg_line("RES> ", "TIMEOUT");
    return ESP8266_RES_TIMEOUT;
}
