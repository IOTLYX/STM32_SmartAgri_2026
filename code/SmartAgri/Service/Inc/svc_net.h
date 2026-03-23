/**
 * @file svc_net.h
 * @brief 网络服务接口
 */

#ifndef SVC_NET_H
#define SVC_NET_H

#include <stdbool.h>
#include "app_data.h"
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化网络服务
 * @param[in] huart_esp ESP 模块通信串口
 * @param[in] huart_dbg 调试输出串口
 * @return 无
 */
void svc_net_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_dbg);

/**
 * @brief 确保网络链路已建立
 * @param[out] net 网络状态数据
 * @return true  网络可用
 * @return false 网络不可用
 */
bool svc_net_ensure_up(app_net_data_t *net);

/**
 * @brief 发布系统快照数据
 * @param[in]  snap 系统快照数据
 * @param[out] net  网络状态数据
 * @return true  发布成功
 * @return false 发布失败
 */
bool svc_net_publish_snapshot(const app_snapshot_t *snap, app_net_data_t *net);

#ifdef __cplusplus
}
#endif

#endif /* SVC_NET_H */