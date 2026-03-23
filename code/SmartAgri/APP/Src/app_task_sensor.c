#include "app_task_sensor.h"
#include "app_data.h"
#include "app_cfg.h"

#include <string.h>
#include "cmsis_os2.h"

#include "adc.h"
#include "i2c.h"

#include "drv_sht30.h"
#include "drv_bh1750.h"
#include "drv_soil_adc.h"
#include "drv_rain_adc.h"

/* 环境数据有效位定义 */
#define ENV_VALID_TEMP_HUM   (1u << 0)  /* 温湿度数据有效 */
#define ENV_VALID_LUX        (1u << 1)  /* 光照数据有效 */
#define ENV_VALID_SOIL       (1u << 2)  /* 土壤湿度数据有效 */
#define ENV_VALID_RAIN       (1u << 3)  /* 雨滴数据有效 */

/**
 * @brief 传感器任务的一次性初始化
 *
 * 初始化本任务依赖的各类传感器驱动，包括：
 * 1. I2C 接口的 SHT30 温湿度传感器
 * 2. I2C 接口的 BH1750 光照传感器
 * 3. ADC 接口的土壤湿度采样驱动
 * 4. ADC 接口的雨滴采样驱动
 */
static void app_sensor_init_once(void)
{
    /* 初始化 SHT30，7bit 地址为 0x44 */
    drv_sht30_init(&hi2c1, 0x44);

    /* 初始化 BH1750，7bit 地址为 0x23 */
    drv_bh1750_init(&hi2c1, 0x23);

    /* 初始化土壤湿度 ADC 采样驱动 */
    drv_soil_init(&hadc1);

    /* 初始化雨滴 ADC 采样驱动 */
    drv_rain_init(&hadc1);
}

/**
 * @brief 传感器任务入口函数
 *
 * 该任务周期性采集环境相关传感器数据，并将结果写入全局数据区。
 * 主要流程包括：
 * 1. 初始化各传感器驱动
 * 2. 周期性采样温湿度、光照、土壤湿度、雨滴数据
 * 3. 将采样值整理为统一环境数据结构
 * 4. 设置对应有效标志位
 * 5. 更新采样时间戳并写入全局快照
 *
 * @param argument 任务入口参数，当前未使用
 */
void sensorTaskStart(void *argument)
{
    (void)argument;

    /* 记录下次任务唤醒时刻，用于实现固定周期采样 */
    uint32_t next_wake = osKernelGetTickCount();

    /* 环境数据缓存 */
    app_env_data_t env;

    /* 初始化各类传感器驱动 */
    app_sensor_init_once();

    /* 启动前先清零环境数据结构 */
    memset(&env, 0, sizeof(env));

    for (;;)
    {
        /* 每轮采样前清空结构体，避免沿用上轮旧数据 */
        memset(&env, 0, sizeof(env));

        /* -------------------- SHT30 温湿度采样 -------------------- */
        (void)drv_sht30_sample();
        {
            sht30_data_t sht;
            memset(&sht, 0, sizeof(sht));

            /* 读取驱动内部缓存的温湿度结果 */
            drv_sht30_get(&sht);

            /* 保存温度和湿度工程值 */
            env.temp_x10 = sht.temp_x10;
            env.hum_x10  = sht.hum_x10;

            /* 仅在驱动返回有效时置位有效标志 */
            if (sht.ok != 0U)
            {
                env.valid_mask |= ENV_VALID_TEMP_HUM;
            }
        }

        /* -------------------- BH1750 光照采样 -------------------- */
        (void)drv_bh1750_sample();
        {
            bh1750_data_t bh;
            memset(&bh, 0, sizeof(bh));

            /* 读取驱动内部缓存的光照结果 */
            drv_bh1750_get(&bh);

            /* 保存光照强度 */
            env.lux = bh.lux;

            /* 仅在驱动返回有效时置位有效标志 */
            if (bh.ok != 0U)
            {
                env.valid_mask |= ENV_VALID_LUX;
            }
        }

        /* -------------------- 土壤湿度采样 -------------------- */
        (void)drv_soil_sample();

        /* 直接读取土壤湿度百分比和原始平均值 */
        env.soil_pct     = drv_soil_get_pct();
        env.soil_raw_avg = drv_soil_get_raw();

        /* 当前设计下土壤通道默认视为有效 */
        env.valid_mask  |= ENV_VALID_SOIL;

        /* -------------------- 雨滴传感器采样 -------------------- */
        (void)drv_rain_sample();
        {
            rain_data_t rain;
            memset(&rain, 0, sizeof(rain));

            /* 读取驱动内部缓存的雨滴结果 */
            drv_rain_get(&rain);

            /* 保存雨滴百分比和原始值 */
            env.rain_pct = rain.pct;
            env.rain_raw = rain.raw;

            /* 仅在驱动返回有效时置位有效标志 */
            if (rain.ok != 0U)
            {
                env.valid_mask |= ENV_VALID_RAIN;
            }
        }

        /* 记录本次采样完成时刻，作为环境数据时间戳 */
        env.sample_tick_ms = osKernelGetTickCount();

        /* 将本轮采样结果写入全局数据区 */
        app_data_set_env(&env);

        /* 使用绝对时基延时，减小采样周期漂移 */
        next_wake += APP_TASK_SENSOR_PERIOD_MS;
        (void)osDelayUntil(next_wake);
    }
}