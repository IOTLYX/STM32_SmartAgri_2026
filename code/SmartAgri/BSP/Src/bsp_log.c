#include "bsp_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *s_huart = NULL;

static uint16_t bsp_strnlen_u16(const char *s, uint16_t max_len)
{
    uint16_t i;

    if (s == NULL)
    {
        return 0U;
    }

    for (i = 0U; i < max_len; i++)
    {
        if (s[i] == '\0')
        {
            break;
        }
    }

    return i;
}

static void _uart_send(const uint8_t *data, uint16_t len)
{
    if (!s_huart || !data || len == 0) return;
    (void)HAL_UART_Transmit(s_huart, (uint8_t *)data, len, BSP_LOG_TX_TIMEOUT_MS);
}

void bsp_log_init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
}

void bsp_log_printf(const char *fmt, ...)
{
    if (!s_huart || !fmt) return;

    char buf[BSP_LOG_BUF_SIZE];

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n <= 0) return;

    uint16_t len = (n >= (int)sizeof(buf)) ? (uint16_t)(sizeof(buf) - 1) : (uint16_t)n;
    _uart_send((const uint8_t *)buf, len);
}

void bsp_log_print_level(char level, const char *fmt, ...)
{
    if (!s_huart || !fmt) return;

    char buf[BSP_LOG_BUF_SIZE];
    int off = 0;

#if BSP_LOG_WITH_TIMESTAMP
    uint32_t ms = HAL_GetTick();
    off = snprintf(buf, sizeof(buf), "[%c %lu] ", level, (unsigned long)ms);
#else
    off = snprintf(buf, sizeof(buf), "[%c] ", level);
#endif

    if (off < 0) return;
    if (off >= (int)sizeof(buf)) off = (int)sizeof(buf) - 1;

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + off, sizeof(buf) - (size_t)off, fmt, ap);
    va_end(ap);

    if (n <= 0) {
        _uart_send((const uint8_t *)buf, bsp_strnlen_u16(buf, (uint16_t)sizeof(buf)));
        return;
    }

    uint16_t len_total = bsp_strnlen_u16(buf, (uint16_t)sizeof(buf));
    _uart_send((const uint8_t *)buf, len_total);
}
