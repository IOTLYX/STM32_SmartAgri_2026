#ifndef APP_TASK_UI_H
#define APP_TASK_UI_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file app_task_ui.h
 * @brief 用户界面任务接口头文件
 *
 * 该任务主要负责系统显示界面的刷新与交互状态维护，
 * 包括 OLED 页面切换、数据显示、提示信息更新以及
 * 界面模式同步等。
 *
 * @note 本文件仅声明 UI 任务对外接口，具体实现位于对应的 .c 文件中。
 */

/**
 * @brief UI 任务入口函数
 *
 * 该函数为 FreeRTOS 用户界面任务入口，由调度器创建并运行，
 * 用于周期性执行界面刷新和显示状态管理。
 *
 * 典型职责包括：
 * 1. 刷新 OLED 显示内容
 * 2. 根据系统状态切换显示页面或模式
 * 3. 显示环境数据、网络状态和告警信息
 * 4. 保持界面显示与全局数据区同步
 *
 * @param argument 任务入口参数，当前未使用
 *
 * @note 该函数应由 RTOS 任务机制启动，不应由业务代码直接调用。
 */
void uiTaskStart(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* APP_TASK_UI_H */