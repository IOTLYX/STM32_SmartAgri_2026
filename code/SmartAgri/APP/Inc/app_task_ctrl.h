#ifndef APP_TASK_CTRL_H
#define APP_TASK_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file app_task_ctrl.h
 * @brief 控制任务接口头文件
 *
 * 该任务作为系统主控制循环，承担类似裸机 while(1) 主循环的职责，
 * 负责各子任务的调度、按键处理以及告警确认等控制逻辑。
 *
 * 任务职责：
 * 1. 作为系统主控调度入口
 * 2. 协调传感器、网络、UI 等子任务执行
 * 3. 处理用户按键输入事件
 * 4. 管理告警确认与消音逻辑
 *
 * FreeRTOS 配置建议：
 * - 任务名：CtrlTask
 * - 栈大小：128 words
 * - 优先级：osPriorityNormal
 *
 * @note 本任务是系统核心调度任务，不建议在其他上下文中直接调用其入口函数。
 */

/**
 * @brief 控制任务入口函数
 *
 * 该函数为 FreeRTOS 任务入口，由调度器启动后持续运行，
 * 用于执行系统主控循环逻辑。
 *
 * 主要流程：
 * 1. 等待系统初始化完成
 * 2. 按周期调度各子功能模块
 *    - 传感器采集
 *    - 网络数据上报
 *    - UI 刷新
 * 3. 响应按键事件
 * 4. 处理告警确认与状态维护
 *
 * @param argument 任务入口参数，当前未使用
 *
 * @note 该函数应由 RTOS 创建任务后自动运行，不应由业务代码直接调用。
 */
void ctrlTaskStart(void *argument);

/**
 * @brief 请求蜂鸣器播放确认提示音
 *
 * 用于向底层蜂鸣器驱动发出一次“确认/应答”提示请求，
 * 常见于用户确认操作、按键响应成功等场景。
 *
 * 触发场景：
 * - 用户按下确认键
 * - 需要给出“操作成功”反馈
 *
 * @note 实际播放通常由 BSP 层非阻塞状态机执行，本函数只负责发起请求。
 */
void app_ctrl_request_buzz_ack(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_TASK_CTRL_H */