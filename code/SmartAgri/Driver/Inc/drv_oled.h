#ifndef __DRV_OLED_H
#define __DRV_OLED_H

#include <stdint.h>
#include "drv_oled_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================= 参数宏定义 ================= */

/**
 * @brief 8x16 字体宽度
 *
 * 该值既用于字体类型判断，也用于计算横向字符偏移。
 */
#define OLED_8X16      8

/**
 * @brief 6x8 字体宽度
 *
 * 该值既用于字体类型判断，也用于计算横向字符偏移。
 */
#define OLED_6X8       6

/**
 * @brief 图形非填充模式
 */
#define OLED_UNFILLED  0

/**
 * @brief 图形填充模式
 */
#define OLED_FILLED    1

/* ================= 函数声明 ================= */

/**
 * @brief 初始化 OLED
 *
 * 完成 OLED 底层硬件初始化和显示配置。
 */
void OLED_Init(void);

/**
 * @brief 刷新整个 OLED 显存到屏幕
 */
void OLED_Update(void);

/**
 * @brief 刷新指定区域到 OLED 屏幕
 *
 * @param X      区域起始 X 坐标
 * @param Y      区域起始 Y 坐标
 * @param Width  区域宽度
 * @param Height 区域高度
 */
void OLED_UpdateArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);

/**
 * @brief 清空整个显存
 */
void OLED_Clear(void);

/**
 * @brief 清空指定区域显存
 *
 * @param X      区域起始 X 坐标
 * @param Y      区域起始 Y 坐标
 * @param Width  区域宽度
 * @param Height 区域高度
 */
void OLED_ClearArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);

/**
 * @brief 整屏反色
 */
void OLED_Reverse(void);

/**
 * @brief 指定区域反色
 *
 * @param X      区域起始 X 坐标
 * @param Y      区域起始 Y 坐标
 * @param Width  区域宽度
 * @param Height 区域高度
 */
void OLED_ReverseArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);

/**
 * @brief 显示单个字符
 *
 * @param X        起始 X 坐标
 * @param Y        起始 Y 坐标
 * @param Char     待显示字符
 * @param FontSize 字体大小，可选 @ref OLED_8X16 或 @ref OLED_6X8
 */
void OLED_ShowChar(int16_t X, int16_t Y, char Char, uint8_t FontSize);

/**
 * @brief 显示字符串
 *
 * @param X        起始 X 坐标
 * @param Y        起始 Y 坐标
 * @param String   待显示字符串
 * @param FontSize 字体大小
 */
void OLED_ShowString(int16_t X, int16_t Y, char *String, uint8_t FontSize);

/**
 * @brief 显示无符号十进制数字
 *
 * @param X        起始 X 坐标
 * @param Y        起始 Y 坐标
 * @param Number   待显示数字
 * @param Length   显示长度
 * @param FontSize 字体大小
 */
void OLED_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);

/**
 * @brief 显示有符号十进制数字
 *
 * @param X        起始 X 坐标
 * @param Y        起始 Y 坐标
 * @param Number   待显示数字
 * @param Length   显示长度
 * @param FontSize 字体大小
 */
void OLED_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize);

/**
 * @brief 显示十六进制数字
 *
 * @param X        起始 X 坐标
 * @param Y        起始 Y 坐标
 * @param Number   待显示数字
 * @param Length   显示长度
 * @param FontSize 字体大小
 */
void OLED_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);

/**
 * @brief 显示二进制数字
 *
 * @param X        起始 X 坐标
 * @param Y        起始 Y 坐标
 * @param Number   待显示数字
 * @param Length   显示长度
 * @param FontSize 字体大小
 */
void OLED_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);

/**
 * @brief 显示浮点数
 *
 * @param X         起始 X 坐标
 * @param Y         起始 Y 坐标
 * @param Number    待显示数字
 * @param IntLength 整数部分长度
 * @param FraLength 小数部分长度
 * @param FontSize  字体大小
 */
void OLED_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize);

/**
 * @brief 显示位图图像
 *
 * @param X      起始 X 坐标
 * @param Y      起始 Y 坐标
 * @param Width  图像宽度
 * @param Height 图像高度
 * @param Image  图像数据指针
 */
void OLED_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image);

/**
 * @brief 类 printf 格式化显示字符串
 *
 * @param X        起始 X 坐标
 * @param Y        起始 Y 坐标
 * @param FontSize 字体大小
 * @param format   格式化字符串
 * @param ...      可变参数
 */
void OLED_Printf(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...);

/**
 * @brief 绘制单个像素点
 *
 * @param X 像素点 X 坐标
 * @param Y 像素点 Y 坐标
 */
void OLED_DrawPoint(int16_t X, int16_t Y);

/**
 * @brief 获取指定像素点状态
 *
 * @param X 像素点 X 坐标
 * @param Y 像素点 Y 坐标
 * @return 像素点状态
 */
uint8_t OLED_GetPoint(int16_t X, int16_t Y);

/**
 * @brief 绘制直线
 *
 * @param X0 起点 X 坐标
 * @param Y0 起点 Y 坐标
 * @param X1 终点 X 坐标
 * @param Y1 终点 Y 坐标
 */
void OLED_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1);

/**
 * @brief 绘制矩形
 *
 * @param X        起始 X 坐标
 * @param Y        起始 Y 坐标
 * @param Width    矩形宽度
 * @param Height   矩形高度
 * @param IsFilled 是否填充，可选 @ref OLED_FILLED 或 @ref OLED_UNFILLED
 */
void OLED_DrawRectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled);

/**
 * @brief 绘制三角形
 *
 * @param X0       顶点 0 的 X 坐标
 * @param Y0       顶点 0 的 Y 坐标
 * @param X1       顶点 1 的 X 坐标
 * @param Y1       顶点 1 的 Y 坐标
 * @param X2       顶点 2 的 X 坐标
 * @param Y2       顶点 2 的 Y 坐标
 * @param IsFilled 是否填充
 */
void OLED_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled);

/**
 * @brief 绘制圆形
 *
 * @param X        圆心 X 坐标
 * @param Y        圆心 Y 坐标
 * @param Radius   半径
 * @param IsFilled 是否填充
 */
void OLED_DrawCircle(int16_t X, int16_t Y, uint8_t Radius, uint8_t IsFilled);

/**
 * @brief 绘制椭圆
 *
 * @param X        椭圆中心 X 坐标
 * @param Y        椭圆中心 Y 坐标
 * @param A        长半轴
 * @param B        短半轴
 * @param IsFilled 是否填充
 */
void OLED_DrawEllipse(int16_t X, int16_t Y, uint8_t A, uint8_t B, uint8_t IsFilled);

/**
 * @brief 绘制圆弧
 *
 * @param X          圆心 X 坐标
 * @param Y          圆心 Y 坐标
 * @param Radius     半径
 * @param StartAngle 起始角度
 * @param EndAngle   结束角度
 * @param IsFilled   是否填充
 */
void OLED_DrawArc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled);

#ifdef __cplusplus
}
#endif

#endif /* __DRV_OLED_H */