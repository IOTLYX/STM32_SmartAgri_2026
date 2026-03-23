#include "bsp_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 当前日志输出绑定的串口句柄 */
static UART_HandleTypeDef *s_huart = NULL;

/**
 * @brief 受长度限制的字符串长度计算函数
 *
 * 在最多检查 max_len 个字符的前提下，返回字符串实际长度，
 * 用于避免在缓冲区未正确结束时越界扫描。
 *
 * @param s       待计算长度的字符串
 * @param max_len 最大检查长度
 * @return uint16_t 实际字符串长度
 */
static uint16_t bsp_strnlen_u16(const char *s, uint16_t max_len)
{
    uint16_t i;

    if (s == NULL)
    {
        return 0U;
    }

    /* 最多检查 max_len 个字符，遇到字符串结束符立即退出 */
    for (i = 0U; i < max_len; i++)
    {
        if (s[i] == '\0')
        {
            break;
        }
    }

    return i;
}

/**
 * @brief 底层串口发送函数
 *
 * 对 HAL_UART_Transmit 做一层简单封装，
 * 统一处理空指针和零长度保护。
 *
 * @param data 待发送数据指针
 * @param len  待发送长度
 */
static void _uart_send(const uint8_t *data, uint16_t len)
{
    if (!s_huart || !data || len == 0)
    {
        return;
    }

    /* 阻塞方式发送日志数据 */
    (void)HAL_UART_Transmit(s_huart, (uint8_t *)data, len, BSP_LOG_TX_TIMEOUT_MS);
}

/**
 * @brief 初始化日志模块
 *
 * 绑定用于日志输出的串口句柄，
 * 后续所有日志均从该串口发送。
 *
 * @param huart 日志串口句柄
 */
void bsp_log_init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
}

/**
 * @brief 输出普通格式化日志
 *
 * 按给定格式将日志写入内部缓冲区，
 * 再通过串口发送出去。
 *
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void bsp_log_printf(const char *fmt, ...)
{
    if (!s_huart || !fmt)
    {
        return;
    }

    char buf[BSP_LOG_BUF_SIZE];

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n <= 0)
    {
        return;
    }

    /* 对截断情况做保护，确保发送长度不超过缓冲区 */
    uint16_t len = (n >= (int)sizeof(buf)) ? (uint16_t)(sizeof(buf) - 1) : (uint16_t)n;
    _uart_send((const uint8_t *)buf, len);
}

/**
 * @brief 输出带等级前缀的格式化日志
 *
 * 根据配置可附加时间戳，最终格式类似：
 * [I 1234] message
 * 或
 * [E] message
 *
 * @param level 日志等级字符，如 I/W/E
 * @param fmt   格式化字符串
 * @param ...   可变参数
 */
void bsp_log_print_level(char level, const char *fmt, ...)
{
    if (!s_huart || !fmt)
    {
        return;
    }

    char buf[BSP_LOG_BUF_SIZE];
    int off = 0;

#if BSP_LOG_WITH_TIMESTAMP
    /* 先写入等级和毫秒时间戳前缀 */
    uint32_t ms = HAL_GetTick();
    off = snprintf(buf, sizeof(buf), "[%c %lu] ", level, (unsigned long)ms);
#else
    /* 仅写入等级前缀 */
    off = snprintf(buf, sizeof(buf), "[%c] ", level);
#endif

    if (off < 0)
    {
        return;
    }

    /* 防止前缀长度异常越界 */
    if (off >= (int)sizeof(buf))
    {
        off = (int)sizeof(buf) - 1;
    }

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + off, sizeof(buf) - (size_t)off, fmt, ap);
    va_end(ap);

    if (n <= 0)
    {
        /* 若正文格式化失败，至少发送已生成的前缀内容 */
        _uart_send((const uint8_t *)buf, bsp_strnlen_u16(buf, (uint16_t)sizeof(buf)));
        return;
    }

    /* 计算最终有效字符串长度并发送 */
    uint16_t len_total = bsp_strnlen_u16(buf, (uint16_t)sizeof(buf));
    _uart_send((const uint8_t *)buf, len_total);
}