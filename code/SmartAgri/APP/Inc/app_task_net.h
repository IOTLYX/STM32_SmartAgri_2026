#ifndef APP_TASK_NET_H
#define APP_TASK_NET_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file app_task_net.h
 * @brief 网络任务接口头文件
 *
 * 该任务主要负责系统网络相关业务的处理，
 * 例如 WiFi 状态维护、MQTT 连接管理、数据上报、
 * 重连处理以及网络状态同步等。
 *
 * @note 本文件仅声明网络任务对外接口，具体实现位于对应的 .c 文件中。
 */

/**
 * @brief 网络任务入口函数
 *
 * 该函数为 FreeRTOS 网络任务入口，由调度器创建并运行，
 * 用于处理系统中的网络通信逻辑。
 *
 * 典型职责包括：
 * 1. 维护网络连接状态
 * 2. 执行 MQTT 连接与重连
 * 3. 进行数据发布与通信状态统计
 * 4. 更新网络状态到全局数据区
 *
 * @param argument 任务入口参数，当前未使用
 *
 * @note 该函数应由 RTOS 任务机制启动，不应由业务代码直接调用。
 */
void netTaskStart(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* APP_TASK_NET_H */