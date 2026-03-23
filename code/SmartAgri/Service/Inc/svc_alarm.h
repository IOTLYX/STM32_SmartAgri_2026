/**
 * @file svc_alarm.h
 * @brief 告警服务接口
 */

#ifndef SVC_ALARM_H
#define SVC_ALARM_H

#include "app_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化告警服务
 * @return 无
 */
void svc_alarm_init(void);

/**
 * @brief 切换告警确认状态
 * @return 无
 * @note 一般用于用户手动确认/消音告警
 */
void svc_alarm_ack_toggle(void);

/**
 * @brief 根据系统快照更新告警状态
 * @param[in]  snap  系统快照数据
 * @param[out] alarm 告警数据输出对象
 * @return 无
 */
void svc_alarm_process(const app_snapshot_t *snap, app_alarm_data_t *alarm);

#ifdef __cplusplus
}
#endif

#endif /* SVC_ALARM_H */