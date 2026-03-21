#ifndef APP_TASK_CTRL_H
#define APP_TASK_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

void ctrlTaskStart(void *argument);
void app_ctrl_request_buzz_ack(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_TASK_CTRL_H */