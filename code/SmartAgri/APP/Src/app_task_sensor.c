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

#define ENV_VALID_TEMP_HUM   (1u << 0)
#define ENV_VALID_LUX        (1u << 1)
#define ENV_VALID_SOIL       (1u << 2)
#define ENV_VALID_RAIN       (1u << 3)

static void app_sensor_init_once(void)
{
    drv_sht30_init(&hi2c1, 0x44);
    drv_bh1750_init(&hi2c1, 0x23);

    drv_soil_init(&hadc1);
    drv_rain_init(&hadc1);
}

void sensorTaskStart(void *argument)
{
    (void)argument;

    uint32_t next_wake = osKernelGetTickCount();
    app_env_data_t env;

    app_sensor_init_once();
    memset(&env, 0, sizeof(env));

    for (;;)
    {
        memset(&env, 0, sizeof(env));

        /* SHT30 */
        (void)drv_sht30_sample();
        {
            sht30_data_t sht;
            memset(&sht, 0, sizeof(sht));
            drv_sht30_get(&sht);

            env.temp_x10 = sht.temp_x10;
            env.hum_x10  = sht.hum_x10;
            if (sht.ok != 0U)
            {
                env.valid_mask |= ENV_VALID_TEMP_HUM;
            }
        }

        /* BH1750 */
        (void)drv_bh1750_sample();
        {
            bh1750_data_t bh;
            memset(&bh, 0, sizeof(bh));
            drv_bh1750_get(&bh);

            env.lux = bh.lux;
            if (bh.ok != 0U)
            {
                env.valid_mask |= ENV_VALID_LUX;
            }
        }

        /* Soil */
        (void)drv_soil_sample();
        env.soil_pct     = drv_soil_get_pct();
        env.soil_raw_avg = drv_soil_get_raw();
        env.valid_mask  |= ENV_VALID_SOIL;

        /* Rain */
        (void)drv_rain_sample();
        {
            rain_data_t rain;
            memset(&rain, 0, sizeof(rain));
            drv_rain_get(&rain);

            env.rain_pct = rain.pct;
            env.rain_raw = rain.raw;
            if (rain.ok != 0U)
            {
                env.valid_mask |= ENV_VALID_RAIN;
            }
        }

        env.sample_tick_ms = osKernelGetTickCount();

        app_data_set_env(&env);

        next_wake += APP_TASK_SENSOR_PERIOD_MS;
        (void)osDelayUntil(next_wake);
    }
}

