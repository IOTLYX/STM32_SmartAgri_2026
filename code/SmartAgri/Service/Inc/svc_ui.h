#ifndef SVC_UI_H
#define SVC_UI_H

#include "app_data.h"

#ifdef __cplusplus
extern "C" {
#endif

void svc_ui_init(void);
void svc_ui_render(const app_snapshot_t *snap);
void svc_ui_force_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* SVC_UI_H */
