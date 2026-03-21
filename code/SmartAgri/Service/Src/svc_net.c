#include "svc_net.h"

#include <stdio.h>
#include <string.h>

#include "app_cfg.h"
#include "drv_esp8266.h"

static UART_HandleTypeDef *s_huart_esp = NULL;
static UART_HandleTypeDef *s_huart_dbg = NULL;
static uint8_t s_esp_inited = 0U;
static uint32_t s_pub_seq = 0U;

static void fmt_x10(char *out, uint32_t out_sz, int16_t v_x10)
{
    int ip;
    int fp;
    int val = (int)v_x10;

    if (val < 0)
    {
        ip = (-val) / 10;
        fp = (-val) % 10;
        (void)snprintf(out, out_sz, "-%d.%d", ip, fp);
    }
    else
    {
        ip = val / 10;
        fp = val % 10;
        (void)snprintf(out, out_sz, "%d.%d", ip, fp);
    }
}

static esp8266_res_t at_send_wait_retry(const char *cmd,
                                        const char *e1,
                                        const char *e2,
                                        uint32_t timeout_ms,
                                        uint8_t retry,
                                        uint32_t retry_delay_ms)
{
    esp8266_res_t r = ESP8266_RES_ERROR;
    uint8_t i;

    for (i = 0U; i <= retry; i++)
    {
        r = ESP8266_SendCmdWait(cmd, e1, e2, timeout_ms, false);
        if (r == ESP8266_RES_OK)
        {
            return r;
        }

        if (i != retry)
        {
            HAL_Delay(retry_delay_ms);
        }
    }

    return r;
}

static esp8266_res_t mqtt_wait_state(uint8_t st, uint32_t timeout_ms)
{
    char expect[32];

    (void)snprintf(expect, sizeof(expect),
                   "+MQTTCONN:%d,%u",
                   APP_MQTT_CLIENT_ID,
                   (unsigned)st);

    return ESP8266_SendCmdWait("AT+MQTTCONN?\r\n", expect, NULL, timeout_ms, false);
}

static esp8266_res_t app_wifi_ensure(void)
{
    char cmd[128];
    esp8266_res_t r;

    r = at_send_wait_retry("AT\r\n", "OK", NULL, 1200U, 1U, 200U);
    if (r != ESP8266_RES_OK) return r;

    r = at_send_wait_retry("ATE0\r\n", "OK", NULL, 1200U, 1U, 200U);
    if (r != ESP8266_RES_OK) return r;

    r = at_send_wait_retry("AT+CWMODE=1\r\n", "OK", NULL, 2000U, 1U, 200U);
    if (r != ESP8266_RES_OK) return r;

    r = ESP8266_SendCmdWait("AT+CWJAP?\r\n", "+CWJAP:", "OK", 1500U, false);
    if (r == ESP8266_RES_OK)
    {
        return ESP8266_RES_OK;
    }

    (void)snprintf(cmd, sizeof(cmd),
                   "AT+CWJAP=\"%s\",\"%s\"\r\n",
                   APP_WIFI_SSID,
                   APP_WIFI_PWD);

    r = ESP8266_SendCmdWait(cmd, "WIFI GOT IP", "OK", 20000U, false);
    return r;
}

static esp8266_res_t app_mqtt_connect_full(void)
{
    esp8266_res_t r;
    char cmd[96];

    (void)ESP8266_SendCmdWait("AT+MQTTCLEAN=0\r\n", "OK", "ERROR", 2000U, false);
    HAL_Delay(200U);

    r = ESP8266_SendCmdWait(APP_MQTT_USERCFG_CMD, "OK", NULL, 4000U, false);
    if (r != ESP8266_RES_OK) return r;

    (void)snprintf(cmd, sizeof(cmd),
                   "AT+MQTTCONNCFG=%d,%d,0,\"\",\"\",0,0\r\n",
                   APP_MQTT_CLIENT_ID,
                   APP_MQTT_KEEPALIVE_S);

    r = ESP8266_SendCmdWait(cmd, "OK", NULL, 3000U, false);
    if (r != ESP8266_RES_OK) return r;

    (void)snprintf(cmd, sizeof(cmd),
                   "AT+MQTTCONN=%d,\"%s\",%d,%d\r\n",
                   APP_MQTT_CLIENT_ID,
                   APP_MQTT_HOST,
                   APP_MQTT_PORT,
                   APP_MQTT_RECONNECT);

    r = ESP8266_SendCmdWait(cmd, "OK", NULL, 6000U, false);
    if (r != ESP8266_RES_OK) return r;

    (void)snprintf(cmd, sizeof(cmd),
                   "AT+MQTTSUB=%d,\"%s\",0\r\n",
                   APP_MQTT_CLIENT_ID,
                   APP_MQTT_TOPIC_REPLY);

    r = ESP8266_SendCmdWait(cmd, "OK", "ALREADY SUBSCRIBE", 3000U, false);
    if (r != ESP8266_RES_OK) return r;

    (void)mqtt_wait_state(6U, 1500U);

    return ESP8266_RES_OK;
}

static esp8266_res_t app_net_ensure_up(void)
{
    esp8266_res_t r;
    char sub_cmd[96];

    if (s_esp_inited == 0U)
    {
        ESP8266_Init(s_huart_esp, s_huart_dbg);
        s_esp_inited = 1U;
    }

    r = app_wifi_ensure();
    if (r != ESP8266_RES_OK) return r;

    if (mqtt_wait_state(6U, 800U) == ESP8266_RES_OK)
    {
        return ESP8266_RES_OK;
    }

    if ((mqtt_wait_state(4U, 800U) == ESP8266_RES_OK) ||
        (mqtt_wait_state(5U, 800U) == ESP8266_RES_OK))
    {
        (void)snprintf(sub_cmd, sizeof(sub_cmd),
                       "AT+MQTTSUB=%d,\"%s\",0\r\n",
                       APP_MQTT_CLIENT_ID,
                       APP_MQTT_TOPIC_REPLY);

        r = ESP8266_SendCmdWait(sub_cmd, "OK", "ALREADY SUBSCRIBE", 3000U, false);
        if (r == ESP8266_RES_OK)
        {
            (void)mqtt_wait_state(6U, 1200U);
            return ESP8266_RES_OK;
        }
    }

    return app_mqtt_connect_full();
}

static esp8266_res_t esp_mqtt_pubraw_json(const char *topic,
                                          const char *json,
                                          uint32_t timeout_ms)
{
    char cmd[200];
    int json_len;
    int n;
    esp8266_res_t r;

    if ((topic == NULL) || (json == NULL))
    {
        return ESP8266_RES_PARAM;
    }

    json_len = (int)strlen(json);
    n = snprintf(cmd, sizeof(cmd),
                 "AT+MQTTPUBRAW=%d,\"%s\",%d,0,0\r\n",
                 APP_MQTT_CLIENT_ID,
                 topic,
                 json_len);
    if ((n <= 0) || (n >= (int)sizeof(cmd)))
    {
        return ESP8266_RES_PARAM;
    }

    r = ESP8266_SendCmdWait(cmd, ">", NULL, timeout_ms, false);
    if (r != ESP8266_RES_OK)
    {
        return r;
    }

    if (ESP8266_SendRawLen((const uint8_t *)json, (uint16_t)json_len, 8000U) != HAL_OK)
    {
        return ESP8266_RES_UART;
    }

    return ESP8266_WaitResp("+MQTTPUB", NULL, timeout_ms, false);
}

void svc_net_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_dbg)
{
    s_huart_esp = huart_esp;
    s_huart_dbg = huart_dbg;
    s_esp_inited = 0U;
    s_pub_seq = 0U;
}

bool svc_net_ensure_up(app_net_data_t *net)
{
    esp8266_res_t r;

    if (net == NULL)
    {
        return false;
    }

    r = app_net_ensure_up();
    if (r == ESP8266_RES_OK)
    {
        net->wifi_ok = 1U;
        net->mqtt_ok = 1U;
        net->last_ok_tick_ms = HAL_GetTick();
        return true;
    }

    net->wifi_ok = 0U;
    net->mqtt_ok = 0U;
    return false;
}

bool svc_net_publish_snapshot(const app_snapshot_t *snap, app_net_data_t *net)
{
    char temp_s[16];
    char hum_s[16];
    char rain_s[16];
    char soil_s[16];
    char id_s[16];
    char json[256];
    int n;
    esp8266_res_t r;

    if ((snap == NULL) || (net == NULL))
    {
        return false;
    }

    fmt_x10(temp_s, sizeof(temp_s), snap->env.temp_x10);
    fmt_x10(hum_s, sizeof(hum_s), (int16_t)snap->env.hum_x10);

    (void)snprintf(rain_s, sizeof(rain_s), "%u.%u",
                   (unsigned)snap->env.rain_pct, 0U);
    (void)snprintf(soil_s, sizeof(soil_s), "%u.%u",
                   (unsigned)snap->env.soil_pct, 0U);

    s_pub_seq++;
    (void)snprintf(id_s, sizeof(id_s), "%lu", (unsigned long)s_pub_seq);

    n = snprintf(json, sizeof(json),
                 "{\"id\":\"%s\",\"version\":\"1.0\",\"params\":{"
                 "\"air_hum\":{\"value\":%s},"
                 "\"lux\":{\"value\":%lu},"
                 "\"rain\":{\"value\":%s},"
                 "\"soil_hum\":{\"value\":%s},"
                 "\"temp\":{\"value\":%s}"
                 "}}",
                 id_s,
                 hum_s,
                 (unsigned long)snap->env.lux,
                 rain_s,
                 soil_s,
                 temp_s);
    if ((n <= 0) || (n >= (int)sizeof(json)))
    {
        return false;
    }

    r = esp_mqtt_pubraw_json(APP_MQTT_TOPIC_POST, json, 3000U);
    if (r == ESP8266_RES_OK)
    {
        net->last_ok_tick_ms = HAL_GetTick();
        return true;
    }

    return false;
}