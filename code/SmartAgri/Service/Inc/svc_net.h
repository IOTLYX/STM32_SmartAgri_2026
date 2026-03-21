#ifndef SVC_NET_H
#define SVC_NET_H

#include <stdbool.h>
#include "app_data.h"
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

void svc_net_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_dbg);

bool svc_net_ensure_up(app_net_data_t *net);
bool svc_net_publish_snapshot(const app_snapshot_t *snap, app_net_data_t *net);

#ifdef __cplusplus
}
#endif

#endif /* SVC_NET_H */