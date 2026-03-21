#ifndef APP_CFG_H
#define APP_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ================= net config ================= */
#define APP_NET_RETRY_COOLDOWN_MS    8000U

#define APP_MQTT_CLIENT_ID           0
#define APP_MQTT_HOST                "mqtts.heclouds.com"
#define APP_MQTT_PORT                1883
#define APP_MQTT_KEEPALIVE_S         120
#define APP_MQTT_RECONNECT           1

#define APP_MQTT_TOPIC_POST          "$sys/Q8a31L7RtU/DEVICE1/thing/property/post"
#define APP_MQTT_TOPIC_REPLY         "$sys/Q8a31L7RtU/DEVICE1/thing/property/post/reply"

#define APP_WIFI_SSID                "CMCC-z2cz"
#define APP_WIFI_PWD                 "e42duuf5"

#define APP_MQTT_USERCFG_CMD \
"AT+MQTTUSERCFG=0,1,\"DEVICE1\",\"Q8a31L7RtU\"," \
"\"version=2018-10-31&res=products%2FQ8a31L7RtU%2Fdevices%2FDEVICE1&et=1781452800&method=md5&sign=Quv2D%2F58G0vkXKwOUnD2Pw%3D%3D\"," \
"0,0,\"\"\r\n"

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

