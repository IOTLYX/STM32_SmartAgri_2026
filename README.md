SmartAgri/
├─ Core/                         # CubeMX 生成
│  ├─ Inc/
│  └─ Src/
├─ Drivers/                      # CubeMX HAL/CMSIS
├─ Middlewares/                  # FreeRTOS
├─ Common/
│  └─ Inc/
│     ├─ app_cfg.h              # 阈值、周期、MQTT/WiFi参数
│     ├─ app_types.h            # 通用枚举/类型
│     └─ app_def.h              # 通用宏、MIN/MAX等
├─ BSP/
│  ├─ Inc/
│  │  ├─ bsp_led.h
│  │  ├─ bsp_buzz.h
│  │  ├─ bsp_key.h
│  │  └─ bsp_log.h
│  └─ Src/
│     ├─ bsp_led.c
│     ├─ bsp_buzz.c
│     ├─ bsp_key.c
│     └─ bsp_log.c
├─ Driver/
│  ├─ Inc/
│  │  ├─ drv_sht30.h
│  │  ├─ drv_bh1750.h
│  │  ├─ drv_soil_adc.h
│  │  ├─ drv_rain_adc.h
│  │  ├─ drv_esp8266.h
│  │  ├─ drv_oled.h
│  │  └─ drv_oled_data.h
│  └─ Src/
│     ├─ drv_sht30.c
│     ├─ drv_bh1750.c
│     ├─ drv_soil_adc.c
│     ├─ drv_rain_adc.c
│     ├─ drv_esp8266.c
│     ├─ drv_oled.c
│     └─ drv_oled_data.c
├─ Service/
│  ├─ Inc/
│  │  ├─ svc_alarm.h            # 告警判断/静音/模式计算
│  │  ├─ svc_net.h              # WiFi/MQTT封装
│  │  └─ svc_ui.h               # OLED绘制/页面切换
│  └─ Src/
│     ├─ svc_alarm.c
│     ├─ svc_net.c
│     └─ svc_ui.c
├─ App/
│  ├─ Inc/
│  │  ├─ app_data.h             # 共享快照
│  │  ├─ app_task_sensor.h
│  │  ├─ app_task_ui.h
│  │  ├─ app_task_net.h
│  │  └─ app_task_ctrl.h
│  └─ Src/
│     ├─ app_data.c
│     ├─ app_task_sensor.c
│     ├─ app_task_ui.c
│     ├─ app_task_net.c
│     └─ app_task_ctrl.c
└─ MDK-ARM/