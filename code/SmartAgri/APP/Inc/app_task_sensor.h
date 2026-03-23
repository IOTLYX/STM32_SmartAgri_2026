#ifndef APP_TASK_SENSOR_H
#define APP_TASK_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file app_task_sensor.h
 * @brief 传感器任务接口头文件
 *
 * 该任务主要负责系统环境数据的采集与预处理，
 * 包括温湿度、光照、土壤湿度、雨滴等传感器数据的读取、
 * 滤波、换算以及结果更新。
 *
 * @note 本文件仅声明传感器任务对外接口，具体实现位于对应的 .c 文件中。
 */

/**
 * @brief 传感器任务入口函数
 *
 * 该函数为 FreeRTOS 传感器任务入口，由调度器创建并运行，
 * 用于周期性完成各类传感器数据采集与状态更新。
 *
 * 典型职责包括：
 * 1. 采集各类原始传感器数据
 * 2. 对 ADC/数字量数据进行滤波或平均处理
 * 3. 将原始值换算为工程值或百分比
 * 4. 更新环境数据到全局数据区
 * 5. 为告警判断和界面显示提供基础输入
 *
 * @param argument 任务入口参数，当前未使用
 *
 * @note 该函数应由 RTOS 任务机制启动，不应由业务代码直接调用。
 */
void sensorTaskStart(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* APP_TASK_SENSOR_H */