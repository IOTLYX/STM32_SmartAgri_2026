#ifndef APP_DATA_H
#define APP_DATA_H

#include <stdint.h>
#include "cmsis_os2.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 环境监测数据结构体
 *
 * 用于保存当前一次采样或最近一次更新的环境数据，
 * 供显示、告警判断、网络上报等模块统一读取。
 */
typedef struct
{
    int16_t  temp_x10;        /* 温度，放大 10 倍存储，例如 25.3C -> 253 */
    uint16_t hum_x10;         /* 湿度，放大 10 倍存储，例如 61.2% -> 612 */
    uint32_t lux;             /* 光照强度，单位 lux */
    uint8_t  soil_pct;        /* 土壤湿度百分比，0~100 */
    uint8_t  rain_pct;        /* 雨量/雨滴百分比，0~100 */

    uint16_t soil_raw_avg;    /* 土壤湿度原始 ADC 平均值 */
    uint16_t rain_raw;        /* 雨滴传感器原始 ADC 值 */

    uint32_t sample_tick_ms;  /* 本次采样对应的系统时基，单位 ms */
    uint8_t  valid_mask;      /* 数据有效标志位掩码，用于标识各字段是否有效 */
} app_env_data_t;

/**
 * @brief 网络状态数据结构体
 *
 * 用于保存 WiFi、MQTT 连接状态以及发布统计信息，
 * 便于网络监控、异常告警和界面显示。
 */
typedef struct
{
    uint8_t  wifi_ok;         /* WiFi 连接状态：0-未连接，1-已连接 */
    uint8_t  mqtt_ok;         /* MQTT 连接状态：0-未连接，1-已连接 */
    int16_t  rssi;            /* 当前信号强度 */
    uint8_t  pub_fail_streak; /* 连续发布失败次数 */

    uint32_t last_ok_tick_ms; /* 最近一次通信成功的系统时基，单位 ms */
    uint32_t pub_ok_cnt;      /* 发布成功累计次数 */
    uint32_t pub_fail_cnt;    /* 发布失败累计次数 */
} app_net_data_t;

/**
 * @brief 告警状态数据结构体
 *
 * 用于统一保存系统当前的各类告警判定结果以及
 * 蜂鸣器、告警灯等执行状态。
 */
typedef struct
{
    uint8_t soil_dry;         /* 土壤干燥标志 */
    uint8_t raining;          /* 下雨标志 */
    uint8_t net_lost;         /* 网络丢失标志 */
    uint8_t alarm_on;         /* 总告警使能/激活标志 */

    uint8_t buzz_mode;        /* 蜂鸣器模式 */
    uint8_t led_alarm_mode;   /* 告警灯模式 */
    uint8_t buzz_muted;       /* 蜂鸣器静音标志 */
    uint8_t reserved;         /* 预留字段，便于后续扩展 */
} app_alarm_data_t;

/**
 * @brief 系统快照数据结构体
 *
 * 将环境数据、网络数据、告警数据以及 UI 状态、
 * 运行时间等信息打包，便于一次性安全读取。
 */
typedef struct
{
    app_env_data_t   env;      /* 环境数据快照 */
    app_net_data_t   net;      /* 网络状态快照 */
    app_alarm_data_t alarm;    /* 告警状态快照 */

    app_ui_mode_t    ui_mode;  /* 当前 UI 模式 */
    uint32_t         uptime_s; /* 系统运行时间，单位 s */
    uint32_t         seq;      /* 数据序号/版本号，用于判断快照是否更新 */
} app_snapshot_t;

/**
 * @brief 初始化应用数据模块
 *
 * 完成内部数据区、互斥资源或默认状态的初始化，
 * 需要在系统启动阶段调用一次。
 */
void app_data_init(void);

/**
 * @brief 更新环境数据
 *
 * @param[in] env 指向待写入环境数据的指针
 */
void app_data_set_env(const app_env_data_t *env);

/**
 * @brief 更新网络状态数据
 *
 * @param[in] net 指向待写入网络状态数据的指针
 */
void app_data_set_net(const app_net_data_t *net);

/**
 * @brief 更新告警状态数据
 *
 * @param[in] alarm 指向待写入告警状态数据的指针
 */
void app_data_set_alarm(const app_alarm_data_t *alarm);

/**
 * @brief 更新当前 UI 模式
 *
 * @param[in] mode 当前界面模式
 */
void app_data_set_ui_mode(app_ui_mode_t mode);

/**
 * @brief 更新系统运行时间
 *
 * @param[in] uptime_s 系统运行秒数
 */
void app_data_set_uptime(uint32_t uptime_s);

/**
 * @brief 获取当前系统数据快照
 *
 * 将内部维护的完整应用数据复制到输出结构体中，
 * 供显示、通信或调试模块统一读取。
 *
 * @param[out] out 用于接收快照数据的输出指针
 */
void app_data_get_snapshot(app_snapshot_t *out);

#ifdef __cplusplus
}
#endif

#endif /* APP_DATA_H */