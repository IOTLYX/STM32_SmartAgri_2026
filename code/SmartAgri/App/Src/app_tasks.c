#include "app_tasks.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "bsp_led.h"
#include "bsp_key.h"
#include "bsp_log.h"
#include "bsp_buzz.h"

#include "drv_bh1750.h"
#include "drv_sht30.h"
#include "drv_soil_adc.h"
#include "drv_rain_adc.h"
#include "drv_esp8266.h"

#include "app_ui.h"

/* ===== 外部句柄（CubeMX 生成）===== */
extern I2C_HandleTypeDef  hi2c1;
extern TIM_HandleTypeDef  htim3;
extern ADC_HandleTypeDef  hadc1;
extern UART_HandleTypeDef huart1;   // 日志串口（DBG）
extern UART_HandleTypeDef huart2;   // ESP8266 串口

/* =========================================================
 * 阈值（调试时只改这里）
 * ========================================================= */
/* 土壤缺水：湿度百分比 <= ON 触发缺水；>= OFF 解除缺水（回差） */
#define TH_SOIL_DRY_ON_PCT     30
#define TH_SOIL_DRY_OFF_PCT    40

/* ★土壤ADC标定（3个探头各自一组：干/湿 两个点） */
#define TH_SOIL1_RAW_DRY       900
#define TH_SOIL1_RAW_WET       300
#define TH_SOIL2_RAW_DRY       900
#define TH_SOIL2_RAW_WET       300
#define TH_SOIL3_RAW_DRY       900
#define TH_SOIL3_RAW_WET       300

/* 雨滴：雨滴百分比 >= ON 判定下雨；<= OFF 判定无雨（回差） */
#define TH_RAIN_ON_PCT         80
#define TH_RAIN_OFF_PCT        30

/* 雨滴ADC标定（raw -> 0~100% 映射用） */
#define TH_RAIN_RAW_DRY        3500
#define TH_RAIN_RAW_WET        1500

/* =========================================================
 * “确认键静音”设置
 * ========================================================= */
#define KEY_MASK_CONFIRM       KEY_MASK_PREV   /* KEY1 当确认键 */

/* =========================================================
 * OneNET / MQTT 参数
 * ========================================================= */
#define MQTT_CLIENT_ID     0
#define MQTT_HOST          "mqtts.heclouds.com"
#define MQTT_PORT          1883
#define MQTT_KEEPALIVE_S   120
#define MQTT_RECONNECT     1     /* 1=AT内置自动重连（建议开） */

#define MQTT_TOPIC_POST    "$sys/Q8a31L7RtU/DEVICE1/thing/property/post"
#define MQTT_TOPIC_REPLY   "$sys/Q8a31L7RtU/DEVICE1/thing/property/post/reply"

#define WIFI_SSID          "COCONUT"
#define WIFI_PWD           "2811332972"

/* 网络状态：1=已连WiFi+MQTT可用；0=不可用 */
static uint8_t  s_net_ok = 0;
/* ESP驱动是否已经初始化过（避免反复 Init） */
static uint8_t  s_esp_inited = 0;

/* 发布失败计数 */
static uint8_t  s_pub_fail_streak = 0;

/* net_ok=0 持续时间（单位 100ms） */
static uint16_t s_net_bad_100ms   = 0;
/* 曾经连上过才报警（避免刚上电乱叫） */
static uint8_t  s_net_ever_ok     = 0;

/* 重连冷却（避免刷屏/卡死） */
#define NET_RETRY_COOLDOWN_MS  8000
static uint32_t s_last_net_try_ms = 0;

/* ===== 软节拍器：每周期只触发一次 + 追赶防漂移 ===== */
typedef struct {
    uint32_t period_ms;
    uint32_t next_ms;
} tick_t;

static tick_t s_t10, s_t100, s_t500, s_t5000;
static app_ui_data_t s_latest = {0};

static void tick_init(tick_t *t, uint32_t period_ms, uint32_t start_delay_ms)
{
    uint32_t now = HAL_GetTick();
    t->period_ms = period_ms;
    t->next_ms   = now + start_delay_ms;
}

static int tick_poll(tick_t *t)
{
    uint32_t now = HAL_GetTick();
    if ((int32_t)(now - t->next_ms) >= 0) {
        do { t->next_ms += t->period_ms; }
        while ((int32_t)(now - t->next_ms) >= 0);
        return 1;
    }
    return 0;
}

/* =========================================================
 * 告警回差小工具
 * ========================================================= */
static bool hyst_low_on_high_off(bool st, uint8_t val, uint8_t on_th, uint8_t off_th)
{
    if (!st) { if (val <= on_th) st = true; }
    else     { if (val >= off_th) st = false; }
    return st;
}

static bool hyst_high_on_low_off(bool st, uint8_t val, uint8_t on_th, uint8_t off_th)
{
    if (!st) { if (val >= on_th) st = true; }
    else     { if (val <= off_th) st = false; }
    return st;
}

static void fmt_x10(char *out, size_t out_sz, int16_t v_x10)
{
    int16_t v = v_x10;
    int sign = (v < 0) ? -1 : 1;
    int a = (v * sign) / 10;
    int b = (v * sign) % 10;
    if (sign < 0) (void)snprintf(out, out_sz, "-%d.%d", a, b);
    else         (void)snprintf(out, out_sz,  "%d.%d", a, b);
}

/* =========================================================
 * AT 发送小工具：失败重试（busy p... / 偶发 ERROR）
 * ========================================================= */
static esp8266_res_t at_send_wait_retry(const char *cmd,
                                       const char *e1,
                                       const char *e2,
                                       uint32_t timeout_ms,
                                       uint8_t retry,
                                       uint32_t retry_delay_ms)
{
    esp8266_res_t r = ESP8266_RES_ERROR;
    for (uint8_t i = 0; i <= retry; i++) {
        r = ESP8266_SendCmdWait(cmd, e1, e2, timeout_ms, false);
        if (r == ESP8266_RES_OK) return r;
        if (i != retry) HAL_Delay(retry_delay_ms);
    }
    return r;
}

/* =========================================================
 * MQTT：查询连接状态（ESP-AT：AT+MQTTCONN? -> +MQTTCONN:<id>,<state>,...）
 * state=6 常见为 “已连接且订阅完成”（你之前日志就是用这个判断的）
 * ========================================================= */
static esp8266_res_t mqtt_wait_state(uint8_t st, uint32_t timeout_ms)
{
    char expect[32];
    (void)snprintf(expect, sizeof(expect), "+MQTTCONN:%d,%u", MQTT_CLIENT_ID, (unsigned)st);
    return ESP8266_SendCmdWait("AT+MQTTCONN?\r\n", expect, NULL, timeout_ms, false);
}

/* =========================================================
 * 发布：PUBRAW（等 +MQTTPUB）
 * ========================================================= */
static esp8266_res_t esp_mqtt_pubraw_json(const char *topic, const char *json, uint32_t timeout_ms)
{
    char cmd[200];

    int len = (int)strlen(json);
    int n = snprintf(cmd, sizeof(cmd),
                     "AT+MQTTPUBRAW=%d,\"%s\",%d,0,0\r\n",
                     MQTT_CLIENT_ID, topic, len);
    if (n <= 0 || n >= (int)sizeof(cmd)) return ESP8266_RES_PARAM;

    /* 1) 发命令，等 '>' */
    esp8266_res_t r = ESP8266_SendCmdWait(cmd, ">", NULL, timeout_ms, false);
    if (r != ESP8266_RES_OK) return r;

    /* 2) 发正文：严格 len 字节 */
    if (ESP8266_SendRawLen((const uint8_t*)json, (uint16_t)len, 8000) != HAL_OK) {
        return ESP8266_RES_UART;
    }

    /* 3) 等 +MQTTPUB:OK / +MQTTPUB:FAIL（都包含 +MQTTPUB） */
    r = ESP8266_WaitResp("+MQTTPUB", NULL, timeout_ms, false);
    return r;
}

/* 把 app_ui_data_t 打包成 OneNET 物模型属性上报 JSON 并发布 */
static esp8266_res_t app_mqtt_publish_thing_property(const app_ui_data_t *d, const char *id_str)
{
    if (!d) return ESP8266_RES_PARAM;

    char temp_s[16], hum_s[16];
    fmt_x10(temp_s, sizeof(temp_s), d->temp_x10);
    fmt_x10(hum_s,  sizeof(hum_s),  d->hum_x10);

    char rain_s[16], soil_s[16];
    (void)snprintf(rain_s, sizeof(rain_s), "%u.%u", (unsigned)d->rain_pct, 0u);
    (void)snprintf(soil_s, sizeof(soil_s), "%u.%u", (unsigned)d->soil_pct, 0u);

    char json[256];
    int n = snprintf(json, sizeof(json),
        "{\"id\":\"%s\",\"version\":\"1.0\",\"params\":{"
            "\"air_hum\":{\"value\":%s},"
            "\"lux\":{\"value\":%u},"
            "\"rain\":{\"value\":%s},"
            "\"soil_hum\":{\"value\":%s},"
            "\"temp\":{\"value\":%s}"
        "}}",
        (id_str && id_str[0]) ? id_str : "123",
        hum_s,
        (unsigned)d->lux,
        rain_s,
        soil_s,
        temp_s
    );
    if (n <= 0 || n >= (int)sizeof(json)) return ESP8266_RES_OVERFLOW;

    return esp_mqtt_pubraw_json(MQTT_TOPIC_POST, json, 3000);
}

/* =========================================================
 * WiFi：确保已连接
 * ========================================================= */
static esp8266_res_t app_wifi_ensure(void)
{
    char cmd[128];

    /* 基本握手 + 关回显 */
    esp8266_res_t r = at_send_wait_retry("AT\r\n", "OK", NULL, 1200, 1, 200);
    if (r != ESP8266_RES_OK) return r;

    r = at_send_wait_retry("ATE0\r\n", "OK", NULL, 1200, 1, 200);
    if (r != ESP8266_RES_OK) return r;

    /* Station 模式 */
    r = at_send_wait_retry("AT+CWMODE=1\r\n", "OK", NULL, 2000, 1, 200);
    if (r != ESP8266_RES_OK) return r;

    /* 已连？优先看 +CWJAP: */
    r = ESP8266_SendCmdWait("AT+CWJAP?\r\n", "+CWJAP:", "OK", 1500, false);
    if (r == ESP8266_RES_OK) {
        return ESP8266_RES_OK;
    }

    /* 重新连接 */
    int n = snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PWD);
    if (n <= 0 || n >= (int)sizeof(cmd)) return ESP8266_RES_PARAM;

    r = ESP8266_SendCmdWait(cmd, "WIFI GOT IP", "OK", 20000, false);
    return r;
}

/* =========================================================
 * MQTT：全量建立（CLEAN -> USERCFG -> CONNCFG -> CONN -> SUB）
 * ========================================================= */
static esp8266_res_t app_mqtt_connect_full(void)
{
    esp8266_res_t r;

    (void)ESP8266_SendCmdWait("AT+MQTTCLEAN=0\r\n", "OK", "ERROR", 2000, false);
    HAL_Delay(200);

    r = ESP8266_SendCmdWait(
        "AT+MQTTUSERCFG=0,1,\"DEVICE1\",\"Q8a31L7RtU\","
        "\"version=2018-10-31&res=products%2FQ8a31L7RtU%2Fdevices%2FDEVICE1&et=1781452800&method=md5&sign=Quv2D%2F58G0vkXKwOUnD2Pw%3D%3D\","
        "0,0,\"\"\r\n",
        "OK", NULL, 4000, false);
    if (r != ESP8266_RES_OK) return r;

    {
        char cmd[96];
        (void)snprintf(cmd, sizeof(cmd),
                       "AT+MQTTCONNCFG=%d,%d,0,\"\",\"\",0,0\r\n",
                       MQTT_CLIENT_ID, MQTT_KEEPALIVE_S);
        r = ESP8266_SendCmdWait(cmd, "OK", NULL, 3000, false);
        if (r != ESP8266_RES_OK) return r;
    }

    {
        char cmd[96];
        (void)snprintf(cmd, sizeof(cmd),
                       "AT+MQTTCONN=%d,\"%s\",%d,%d\r\n",
                       MQTT_CLIENT_ID, MQTT_HOST, MQTT_PORT, MQTT_RECONNECT);
        r = ESP8266_SendCmdWait(cmd, "OK", NULL, 6000, false);
        if (r != ESP8266_RES_OK) return r;
    }

    r = ESP8266_SendCmdWait(
        "AT+MQTTSUB=0,\"" MQTT_TOPIC_REPLY "\",0\r\n",
        "OK", "ALREADY SUBSCRIBE", 3000, false);

    if (r != ESP8266_RES_OK) return r;

    /* 等待进入 state=6（如果你固件不是6，也可以把这句删掉） */
    (void)mqtt_wait_state(6, 1500);

    return ESP8266_RES_OK;
}

/* =========================================================
 * 网络：确保 WiFi + MQTT 可用
 *   - 先确保 WiFi
 *   - MQTT：先查状态能用就不折腾；否则全量重建
 * ========================================================= */
static esp8266_res_t app_net_ensure_up(void)
{
    esp8266_res_t r;

    if (!s_esp_inited) {
        ESP8266_Init(&huart2, &huart1);
        s_esp_inited = 1;
    }

    r = app_wifi_ensure();
    if (r != ESP8266_RES_OK) return r;

    /* MQTT 已经“处于可用态”就直接返回 */
    if (mqtt_wait_state(6, 800) == ESP8266_RES_OK) return ESP8266_RES_OK;
    if (mqtt_wait_state(4, 800) == ESP8266_RES_OK || mqtt_wait_state(5, 800) == ESP8266_RES_OK) {
        /* 可能已连未订阅：补一次订阅 */
        r = ESP8266_SendCmdWait(
            "AT+MQTTSUB=0,\"" MQTT_TOPIC_REPLY "\",0\r\n",
            "OK", "ALREADY SUBSCRIBE", 3000, false);
        if (r == ESP8266_RES_OK) {
            (void)mqtt_wait_state(6, 1200);
            return ESP8266_RES_OK;
        }
    }

    /* 其他状态（如3断开）：走全量重建 */
    r = app_mqtt_connect_full();
    return r;
}

/* =========================================================
 * “确认键静音”内部状态
 * ========================================================= */
static volatile uint8_t s_req_buzz_ack = 0;
static bool s_buzz_muted = false;
static uint8_t s_buzz_muted_mode = BUZZ_MODE_OFF;

/* ===== 任务槽 ===== */
static void task_10ms(void)
{
    bsp_key_scan_10ms();
    uint8_t trg = bsp_key_trg_get();

    if (trg) {
        app_ui_on_key_trg(trg);

        if (trg & KEY_MASK_PREV) LOGI("KEY1\r\n");
        if (trg & KEY_MASK_NEXT) LOGI("KEY2\r\n");
        if (trg & KEY_MASK_MODE) LOGI("KEY3\r\n");
    }

    if (trg & KEY_MASK_CONFIRM) {
        s_req_buzz_ack = 1;
        LOGI("MUTE_REQ\r\n");
    }

    bsp_led_step_10ms();
    bsp_buzz_step_10ms();
}

static void task_100ms(void)
{
    static uint32_t up_cnt = 0;

    static bool s_dry_alarm  = false;
    static bool s_rain_alarm = false;

    static uint8_t s_last_led_alm = 0xFF;
    static uint8_t s_last_buzz    = 0xFF;
    static uint8_t s_last_net_ok  = 0xFF;

    app_ui_data_t d = {0};

    /* 1) 温湿度 */
    drv_sht30_sample();
    sht30_data_t sht;
    drv_sht30_get(&sht);
    d.temp_x10 = sht.temp_x10;
    d.hum_x10  = sht.hum_x10;

    /* 2) 光照 */
    drv_bh1750_sample();
    bh1750_data_t bh;
    drv_bh1750_get(&bh);
    d.lux = bh.lux;

    /* 3) 雨滴 */
    drv_rain_sample();
    rain_data_t rr;
    drv_rain_get(&rr);
    d.rain_pct = rr.pct;

    /* 4) 土壤 */
    drv_soil_sample();
    d.soil_pct = drv_soil_get_pct();

    /* 5) 网络状态 */
    d.wifi_ok = s_net_ok;
    if (d.wifi_ok) {
        s_net_ever_ok = 1;
        s_net_bad_100ms = 0;
    } else {
        if (s_net_bad_100ms < 60000) s_net_bad_100ms++;
    }

    d.rssi    = 0;
    d.up_cnt  = up_cnt++;

    /* 6) 阈值（回差） */
    s_dry_alarm  = hyst_low_on_high_off(s_dry_alarm,  d.soil_pct, TH_SOIL_DRY_ON_PCT, TH_SOIL_DRY_OFF_PCT);
    s_rain_alarm = hyst_high_on_low_off(s_rain_alarm, d.rain_pct, TH_RAIN_ON_PCT,    TH_RAIN_OFF_PCT);

    d.alarm_on = (s_dry_alarm || s_rain_alarm || (!d.wifi_ok)) ? 1 : 0;

    /* 7) LED */
    if (s_last_net_ok != d.wifi_ok) {
        bsp_led_set_net_ok(d.wifi_ok ? true : false);
        s_last_net_ok = d.wifi_ok;
    }

    uint8_t led_alm_mode = (d.alarm_on ? LED_ALM_DRY : LED_ALM_NONE);
    if (s_last_led_alm != led_alm_mode) {
        bsp_led_set_alarm(led_alm_mode);
        s_last_led_alm = led_alm_mode;
    }

    /* 8) 蜂鸣器模式 */
    uint8_t buzz_mode_raw = BUZZ_MODE_OFF;
    if (s_dry_alarm)           buzz_mode_raw = BUZZ_MODE_DRY;
    else if (s_rain_alarm)     buzz_mode_raw = BUZZ_MODE_RAIN;
    else if (!d.wifi_ok && s_net_ever_ok && s_net_bad_100ms >= 50)  /* 5s */
        buzz_mode_raw = BUZZ_MODE_NET_LOST;
    else
        buzz_mode_raw = BUZZ_MODE_OFF;

    if (s_req_buzz_ack) {
        s_req_buzz_ack = 0;

        if (buzz_mode_raw == BUZZ_MODE_OFF) {
            s_buzz_muted = false;
            s_buzz_muted_mode = BUZZ_MODE_OFF;
        } else {
            if (!s_buzz_muted) {
                s_buzz_muted = true;
                s_buzz_muted_mode = buzz_mode_raw;
            } else {
                s_buzz_muted = false;
                s_buzz_muted_mode = BUZZ_MODE_OFF;
            }
        }
        LOGI("MUTE=%d mute_mode=%d raw=%d\r\n", s_buzz_muted, s_buzz_muted_mode, buzz_mode_raw);
    }

    if (buzz_mode_raw == BUZZ_MODE_OFF) {
        s_buzz_muted = false;
        s_buzz_muted_mode = BUZZ_MODE_OFF;
    }
    if (s_buzz_muted && buzz_mode_raw != BUZZ_MODE_OFF && buzz_mode_raw != s_buzz_muted_mode) {
        s_buzz_muted = false;
        s_buzz_muted_mode = BUZZ_MODE_OFF;
    }

    uint8_t buzz_mode = buzz_mode_raw;
    if (s_buzz_muted && buzz_mode_raw != BUZZ_MODE_OFF && buzz_mode_raw == s_buzz_muted_mode) {
        buzz_mode = BUZZ_MODE_OFF;
    }

    if (s_last_buzz != buzz_mode) {
        bsp_buzz_set_mode(buzz_mode);
        s_last_buzz = buzz_mode;
    }

    /* 9) UI */
    app_ui_set_data(&d);
    s_latest = d;
}

static void task_500ms(void)
{
    app_ui_refresh();
    bsp_led_toggle(LED_HEART);
}

static void task_5s(void)
{
    static uint32_t s_id = 0;
    char idbuf[16];
    (void)snprintf(idbuf, sizeof(idbuf), "%lu", (unsigned long)(++s_id));

    /* 网络不OK：冷却重试，避免疯狂重连刷屏/卡主循环 */
    if (!s_net_ok) {
        uint32_t now = HAL_GetTick();
        if ((uint32_t)(now - s_last_net_try_ms) < NET_RETRY_COOLDOWN_MS) {
            LOGI("PUB skip (net not ready)\r\n");
            return;
        }
        s_last_net_try_ms = now;

        esp8266_res_t ir = app_net_ensure_up();
        s_net_ok = (ir == ESP8266_RES_OK) ? 1 : 0;
        LOGI("[NET] ensure r=%d net_ok=%d\r\n", (int)ir, (int)s_net_ok);

        if (!s_net_ok) {
            LOGI("PUB skip (net not ready)\r\n");
            return;
        }
    }

    /* 正常发布 */
    esp8266_res_t r = app_mqtt_publish_thing_property(&s_latest, idbuf);
    LOGI("PUB r=%d\r\n", (int)r);

    /* 发布失败：连续两次失败才判定断网 */
    if (r != ESP8266_RES_OK) {
        if (s_pub_fail_streak < 255) s_pub_fail_streak++;
        LOGI("PUB fail streak=%u\r\n", (unsigned)s_pub_fail_streak);

        if (s_pub_fail_streak >= 2) {
            s_pub_fail_streak = 0;
            s_net_ok = 0;
            LOGI("PUB fail -> net_ok=0\r\n");
        }
    } else {
        s_pub_fail_streak = 0;
    }
}

void app_tasks_init(void)
{
    /* ===== BSP 初始化 ===== */
    bsp_log_init(&huart1);
    bsp_led_init();
    bsp_key_init();
    bsp_buzz_init(&htim3, TIM_CHANNEL_1);

    /* ===== 传感器驱动初始化 ===== */
    drv_bh1750_init(&hi2c1, 0x23);
    drv_sht30_init(&hi2c1, 0x44);

    /* ===== 土壤 ADC ===== */
    drv_soil_init(&hadc1);

    /* ===== 雨滴 ADC ===== */
    drv_rain_init(&hadc1);

    rain_cfg_t rcfg = {0};
    rcfg.raw_dry = TH_RAIN_RAW_DRY;
    rcfg.raw_wet = TH_RAIN_RAW_WET;
    rcfg.rain_on_pct  = TH_RAIN_ON_PCT;
    rcfg.rain_off_pct = TH_RAIN_OFF_PCT;
    rcfg.en_hysteresis = 1;
    drv_rain_set_cfg(&rcfg);

    /* ===== UI 初始化 ===== */
    app_ui_init();

    /* ===== 上电连一次网络 ===== */
    esp8266_res_t r = app_net_ensure_up();
    s_net_ok = (r == ESP8266_RES_OK) ? 1 : 0;
    LOGI("[NET] init r=%d net_ok=%d\r\n", (int)r, (int)s_net_ok);

    /* ===== 任务节拍初始化 ===== */
    tick_init(&s_t10,   10,   10);
    tick_init(&s_t100,  100,  20);
    tick_init(&s_t500,  500,  30);
    tick_init(&s_t5000, 5000, 40);

    LOGI("app_tasks_init ok\r\n");
}

void app_tasks_run(void)
{
    if (tick_poll(&s_t10))   task_10ms();
    if (tick_poll(&s_t100))  task_100ms();
    if (tick_poll(&s_t500))  task_500ms();
    if (tick_poll(&s_t5000)) task_5s();
}




