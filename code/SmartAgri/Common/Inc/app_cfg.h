#ifndef APP_CFG_H
#define APP_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

/* task period */
#define APP_TASK_SENSOR_PERIOD_MS      100U
#define APP_TASK_UI_SCAN_PERIOD_MS      10U
#define APP_TASK_UI_REFRESH_MS         200U
#define APP_TASK_CTRL_PERIOD_MS         10U
#define APP_TASK_NET_PERIOD_MS        5000U

/* alarm threshold: first version */
#define APP_TH_SOIL_DRY_ON_PCT         30U
#define APP_TH_SOIL_DRY_OFF_PCT        40U

#define APP_TH_RAIN_ON_PCT             80U
#define APP_TH_RAIN_OFF_PCT            30U

#define APP_NET_LOST_ALM_DELAY_MS    5000U

#ifdef __cplusplus
}
#endif

#endif /* APP_CFG_H */

