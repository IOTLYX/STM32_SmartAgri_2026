#ifndef APP_CFG_H
#define APP_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 网络重试冷却时间，单位 ms
 *
 * 当网络连接失败后，至少等待该时间后再进行下一轮重连，
 * 用于避免频繁重试导致主循环阻塞或模组压力过大。
 */
#define APP_NET_RETRY_COOLDOWN_MS    8000U

/**
 * @brief MQTT 客户端索引号
 *
 * 对应 ESP8266 AT 固件中的 MQTT 连接编号。
 * 当前工程固定使用 0 号客户端。
 */
#define APP_MQTT_CLIENT_ID           0

/**
 * @brief MQTT 服务器地址
 */
#define APP_MQTT_HOST                "mqtts.heclouds.com"

/**
 * @brief MQTT 服务器端口号
 */
#define APP_MQTT_PORT                1883

/**
 * @brief MQTT 保活时间，单位 s
 *
 * 客户端在该周期内需要与服务器保持通信，
 * 超时可能被服务器判定为离线。
 */
#define APP_MQTT_KEEPALIVE_S         120

/**
 * @brief 是否启用 MQTT 自动重连
 *
 * 1：启用自动重连
 * 0：关闭自动重连
 */
#define APP_MQTT_RECONNECT           1

/**
 * @brief 设备属性上报主题
 *
 * 设备通过该主题向 OneNET 上报传感器数据或属性数据。
 */
#define APP_MQTT_TOPIC_POST          "$sys/Q8a31L7RtU/DEVICE1/thing/property/post"

/**
 * @brief 设备属性上报回复主题
 *
 * 平台对属性上报结果的应答会返回到该主题。
 */
#define APP_MQTT_TOPIC_REPLY         "$sys/Q8a31L7RtU/DEVICE1/thing/property/post/reply"

/**
 * @brief Wi-Fi 名称
 */
#define APP_WIFI_SSID                "CMCC-z2cz"

/**
 * @brief Wi-Fi 密码
 */
#define APP_WIFI_PWD                 "e42duuf5"

/**
 * @brief MQTT 用户配置 AT 指令
 *
 * 用于配置 ESP8266 MQTT 客户端的认证信息、设备 ID、
 * 产品 ID 以及 OneNET 鉴权参数。
 *
 * 说明：
 * - client_id：DEVICE1
 * - username ：Q8a31L7RtU
 * - password ：OneNET 鉴权签名串
 *
 * 注意：
 * 该字符串中包含完整鉴权信息，后续若设备、产品或签名变更，
 * 需要同步更新此宏定义。
 */
#define APP_MQTT_USERCFG_CMD \
"AT+MQTTUSERCFG=0,1,\"DEVICE1\",\"Q8a31L7RtU\"," \
"\"version=2018-10-31&res=products%2FQ8a31L7RtU%2Fdevices%2FDEVICE1&et=1781452800&method=md5&sign=Quv2D%2F58G0vkXKwOUnD2Pw%3D%3D\"," \
"0,0,\"\"\r\n"

/**
 * @brief 传感器采样任务周期，单位 ms
 *
 * 周期性采集土壤湿度、雨量、温湿度等传感器数据。
 */
#define APP_TASK_SENSOR_PERIOD_MS      100U

/**
 * @brief UI 按键扫描任务周期，单位 ms
 *
 * 用于检测按键输入、页面切换等人机交互操作。
 */
#define APP_TASK_UI_SCAN_PERIOD_MS      10U

/**
 * @brief UI 刷新任务周期，单位 ms
 *
 * 控制 OLED 或其他显示界面的刷新频率，
 * 避免刷新过快占用过多 CPU 时间。
 */
#define APP_TASK_UI_REFRESH_MS         200U

/**
 * @brief 控制任务周期，单位 ms
 *
 * 用于执行蜂鸣器、LED、继电器、电磁阀等控制逻辑。
 */
#define APP_TASK_CTRL_PERIOD_MS         10U

/**
 * @brief 网络任务周期，单位 ms
 *
 * 用于网络状态维护、MQTT 保活、重连检测、
 * 数据周期上报等相关逻辑。
 */
#define APP_TASK_NET_PERIOD_MS        5000U

/**
 * @brief 土壤干燥报警开启阈值，单位 %
 *
 * 当土壤湿度低于该阈值时，进入“土壤干燥”状态。
 */
#define APP_TH_SOIL_DRY_ON_PCT         30U

/**
 * @brief 土壤干燥报警关闭阈值，单位 %
 *
 * 当土壤湿度回升到该阈值及以上时，退出“土壤干燥”状态。
 *
 * 说明：
 * 该值高于开启阈值，用于构成迟滞判断，避免阈值附近反复抖动。
 */
#define APP_TH_SOIL_DRY_OFF_PCT        40U

/**
 * @brief 降雨报警开启阈值，单位 %
 *
 * 当雨量传感器值高于该阈值时，判定为下雨。
 */
#define APP_TH_RAIN_ON_PCT             80U

/**
 * @brief 降雨报警关闭阈值，单位 %
 *
 * 当雨量传感器值低于该阈值时，判定为未下雨。
 *
 * 说明：
 * 该值低于开启阈值，用于构成迟滞判断，避免状态频繁切换。
 */
#define APP_TH_RAIN_OFF_PCT            30U

/**
 * @brief 网络丢失报警延迟时间，单位 ms
 *
 * 当网络断开持续超过该时间后，才触发网络丢失报警，
 * 用于过滤瞬时抖动或短时重连过程。
 */
#define APP_NET_LOST_ALM_DELAY_MS    5000U

#ifdef __cplusplus
}
#endif

#endif /* APP_CFG_H */