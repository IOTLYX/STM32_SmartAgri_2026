#ifndef APP_TYPES_H
#define APP_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UI 界面模式枚举
 *
 * 用于表示当前 OLED 或应用界面的显示模式，
 * 上层可根据该状态切换不同页面内容。
 */
typedef enum
{
    /**
     * @brief 监控页面
     *
     * 主要显示传感器实时数据、设备运行状态等信息。
     */
    APP_UI_MODE_MONITOR = 0,

    /**
     * @brief 状态页面
     *
     * 主要显示网络连接、告警状态、执行器状态等信息。
     */
    APP_UI_MODE_STATUS,

    /**
     * @brief 信息页面
     *
     * 主要显示设备附加信息、调试信息或版本信息等内容。
     */
    APP_UI_MODE_INFO,

    /**
     * @brief UI 页面数量
     *
     * 用于页面循环切换、边界判断或数组大小定义。
     */
    APP_UI_MODE_COUNT
} app_ui_mode_t;

#ifdef __cplusplus
}
#endif

#endif /* APP_TYPES_H */