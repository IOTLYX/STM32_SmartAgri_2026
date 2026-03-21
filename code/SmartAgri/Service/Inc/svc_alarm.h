#ifndef SVC_ALARM_H
#define SVC_ALARM_H

#include "app_data.h"

#ifdef __cplusplus
extern "C" {
#endif

void svc_alarm_init(void);
void svc_alarm_ack_toggle(void);
void svc_alarm_process(const app_snapshot_t *snap, app_alarm_data_t *alarm);

#ifdef __cplusplus
}
#endif

#endif /* SVC_ALARM_H */