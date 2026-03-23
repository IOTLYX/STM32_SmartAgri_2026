/**
 * @file svc_ui.h
 * @brief UI 渲染服务接口
 */

#ifndef SVC_UI_H
#define SVC_UI_H

#include "app_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 UI 服务
 * @return 无
 * @note 一般完成显示状态复位、首页准备等初始化工作
 */
void svc_ui_init(void);

/**
 * @brief 根据系统快照渲染当前页面
 * @param[in] snap 系统快照数据
 * @return 无
 * @note 建议在主循环或周期任务中调用
 */
void svc_ui_render(const app_snapshot_t *snap);

/**
 * @brief 强制立即刷新屏幕
 * @return 无
 * @note 可用于页面切换、重要告警显示等场景
 */
void svc_ui_force_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* SVC_UI_H */