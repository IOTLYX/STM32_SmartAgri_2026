#ifndef __OLED_DATA_H
#define __OLED_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================= 字符集定义 ================= */

/**
 * @brief 字符集选择
 *
 * 以下两个宏定义仅可启用其中一个。
 * 当前默认使用 GB2312 字符集。
 */
/* #define OLED_CHARSET_UTF8 */     /**< 定义字符集为 UTF8 */
#define OLED_CHARSET_GB2312         /**< 定义字符集为 GB2312 */

/**
 * @brief 汉字字模基本单元
 *
 * 用于存放一个汉字的索引和对应字模数据。
 */
typedef struct
{
#ifdef OLED_CHARSET_UTF8
    char Index[5];      /**< 汉字索引，UTF8 编码最大预留 5 字节 */
#endif

#ifdef OLED_CHARSET_GB2312
    char Index[3];      /**< 汉字索引，GB2312 编码预留 3 字节 */
#endif

    uint8_t Data[32];   /**< 16x16 汉字字模数据 */
} ChineseCell_t;

/* ================= 字模数据声明 ================= */

/**
 * @brief ASCII 8x16 字模表
 */
extern const uint8_t OLED_F8x16[][16];

/**
 * @brief ASCII 6x8 字模表
 */
extern const uint8_t OLED_F6x8[][6];

/**
 * @brief 16x16 汉字字模表
 */
extern const ChineseCell_t OLED_CF16x16[];

/* ================= 图像数据声明 ================= */

/**
 * @brief 二极管图像数据
 */
extern const uint8_t Diode[];

/* 按照上面的格式，在此处继续添加新的图像数据声明 */

#ifdef __cplusplus
}
#endif

#endif /* __OLED_DATA_H */