#ifndef APP_DATA_H
#define APP_DATA_H

#include <stdint.h>
#include "cmsis_os2.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int16_t  temp_x10;        /* 25.3C -> 253 */
    uint16_t hum_x10;         /* 61.2% -> 612 */
    uint32_t lux;
    uint8_t  soil_pct;
    uint8_t  rain_pct;

    uint16_t soil_raw_avg;
    uint16_t rain_raw;

    uint32_t sample_tick_ms;
    uint8_t  valid_mask;
} app_env_data_t;

typedef struct
{
    uint8_t  wifi_ok;
    uint8_t  mqtt_ok;
    int16_t  rssi;
    uint8_t  pub_fail_streak;

    uint32_t last_ok_tick_ms;
    uint32_t pub_ok_cnt;
    uint32_t pub_fail_cnt;
} app_net_data_t;

typedef struct
{
    uint8_t soil_dry;
    uint8_t raining;
    uint8_t net_lost;
    uint8_t alarm_on;

    uint8_t buzz_mode;
    uint8_t led_alarm_mode;
    uint8_t buzz_muted;
    uint8_t reserved;
} app_alarm_data_t;

typedef struct
{
    app_env_data_t   env;
    app_net_data_t   net;
    app_alarm_data_t alarm;

    app_ui_mode_t    ui_mode;
    uint32_t         uptime_s;
    uint32_t         seq;
} app_snapshot_t;

void app_data_init(void);

void app_data_set_env(const app_env_data_t *env);
void app_data_set_net(const app_net_data_t *net);
void app_data_set_alarm(const app_alarm_data_t *alarm);
void app_data_set_ui_mode(app_ui_mode_t mode);
void app_data_set_uptime(uint32_t uptime_s);

void app_data_get_snapshot(app_snapshot_t *out);

#ifdef __cplusplus
}
#endif

#endif /* APP_DATA_H */

