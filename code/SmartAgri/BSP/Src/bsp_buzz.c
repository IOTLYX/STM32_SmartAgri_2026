#include "bsp_buzz.h"

/* ====== 可根据实际听感微调的蜂鸣器参数 ====== */
#define BUZZ_FREQ_HIGH_HZ      2500u   /* 缺水高频音 */
#define BUZZ_FREQ_LOW_HZ       1200u   /* 缺水低频音 */
#define BUZZ_FREQ_RAIN_HZ      2000u   /* 降雨提示音频率 */
#define BUZZ_FREQ_NET_HZ       1500u   /* 断网提示音频率 */

/* TIM3: 72MHz / (PSC+1=72) = 1MHz 计数频率 */
#define BUZZ_TMR_CNT_HZ        1000000u

/**
 * @brief 蜂鸣器节奏步骤定义
 */
typedef struct
{
    uint16_t dur_ms;   /* 当前步骤持续时间，单位 ms */
    uint16_t freq_hz;  /* 发声频率，0 表示静音 */
    uint8_t  on;       /* 1 表示发声，0 表示静音 */
} buzz_step_t;

/* PWM 输出资源 */
static TIM_HandleTypeDef *s_htim = NULL;   /* 蜂鸣器使用的定时器句柄 */
static uint32_t s_ch = 0;                  /* 蜂鸣器使用的 PWM 通道 */
static buzz_mode_t s_mode = BUZZ_MODE_OFF; /* 当前蜂鸣器模式 */

/* 节奏状态机变量 */
static uint8_t s_idx = 0;          /* 当前节奏表索引 */
static uint16_t s_remain_ms = 0;   /* 当前步骤剩余时间 */

/**
 * @brief 应用蜂鸣器输出参数
 *
 * 根据开关状态和目标频率，动态设置 PWM 的 ARR 和 CCR，
 * 实现不同音调输出或静音控制。
 *
 * @param on      1-发声，0-静音
 * @param freq_hz 发声频率，单位 Hz，0 表示静音
 */
static void _buzz_apply(uint8_t on, uint16_t freq_hz)
{
    if (!s_htim)
    {
        return;
    }

    if ((!on) || (freq_hz == 0U))
    {
        /* 通过将比较值设为 0 实现静音 */
        __HAL_TIM_SET_COMPARE(s_htim, s_ch, 0);
        return;
    }

    /* ARR = (timer_clk / freq) - 1 */
    uint32_t arr = (BUZZ_TMR_CNT_HZ / (uint32_t)freq_hz);
    if (arr == 0U)
    {
        arr = 1U;
    }
    arr -= 1U;

    if (arr > 0xFFFFu)
    {
        arr = 0xFFFFu;
    }

    /* 更新自动重装值和计数器，切换输出频率 */
    __HAL_TIM_SET_AUTORELOAD(s_htim, (uint16_t)arr);
    __HAL_TIM_SET_COUNTER(s_htim, 0);

    /* 设置为约 50% 占空比 */
    uint16_t ccr = (uint16_t)((arr + 1u) / 2u);
    __HAL_TIM_SET_COMPARE(s_htim, s_ch, ccr);
}

/* ====== 蜂鸣器节奏表 ======
 * 说明：
 * - 缺水：高频响 -> 停 -> 低频响 -> 停，循环
 * - 降雨：三短响 + 长停，循环
 * - 断网：短响 + 长停，循环
 */
static const buzz_step_t s_pat_dry[] =
{
    {120, BUZZ_FREQ_HIGH_HZ, 1},
    { 60, 0,                 0},
    {120, BUZZ_FREQ_LOW_HZ,  1},
    { 60, 0,                 0},
};

static const buzz_step_t s_pat_rain[] =
{
    { 80, BUZZ_FREQ_RAIN_HZ, 1},
    { 80, 0,                 0},
    { 80, BUZZ_FREQ_RAIN_HZ, 1},
    { 80, 0,                 0},
    { 80, BUZZ_FREQ_RAIN_HZ, 1},
    {400, 0,                 0}, /* 长停顿 */
};

static const buzz_step_t s_pat_net[] =
{
    {200, BUZZ_FREQ_NET_HZ, 1},
    {800, 0,                0},
};

/**
 * @brief 根据蜂鸣器模式获取对应节奏表
 *
 * @param m   蜂鸣器模式
 * @param len 输出节奏表长度
 * @return const buzz_step_t* 对应节奏表首地址，若无效则返回 NULL
 */
static const buzz_step_t* _get_pat(buzz_mode_t m, uint8_t *len)
{
    switch (m)
    {
    case BUZZ_MODE_DRY:
        *len = (uint8_t)(sizeof(s_pat_dry) / sizeof(s_pat_dry[0]));
        return s_pat_dry;

    case BUZZ_MODE_RAIN:
        *len = (uint8_t)(sizeof(s_pat_rain) / sizeof(s_pat_rain[0]));
        return s_pat_rain;

    case BUZZ_MODE_NET_LOST:
        *len = (uint8_t)(sizeof(s_pat_net) / sizeof(s_pat_net[0]));
        return s_pat_net;

    default:
        *len = 0;
        return NULL;
    }
}

/**
 * @brief 初始化蜂鸣器驱动
 *
 * 保存 PWM 资源，启动 PWM 输出，并默认静音。
 *
 * @param htim    定时器句柄
 * @param channel PWM 通道
 */
void bsp_buzz_init(TIM_HandleTypeDef *htim, uint32_t channel)
{
    s_htim = htim;
    s_ch = channel;
    s_mode = BUZZ_MODE_OFF;
    s_idx = 0;
    s_remain_ms = 0;

    if (s_htim)
    {
        /* 启动 PWM，但先保持静音 */
        HAL_TIM_PWM_Start(s_htim, s_ch);
        _buzz_apply(0, 0);
    }
}

/**
 * @brief 立即关闭蜂鸣器
 *
 * 清空当前节奏状态，并立即静音。
 */
void bsp_buzz_off(void)
{
    s_mode = BUZZ_MODE_OFF;
    s_idx = 0;
    s_remain_ms = 0;
    _buzz_apply(0, 0);
}

/**
 * @brief 设置蜂鸣器工作模式
 *
 * 若模式变化，则重置节奏状态机，从新模式起始步骤开始执行。
 *
 * @param mode 目标蜂鸣器模式
 */
void bsp_buzz_set_mode(buzz_mode_t mode)
{
    if (s_mode == mode)
    {
        return;
    }

    s_mode = mode;
    s_idx = 0;
    s_remain_ms = 0;

    if (s_mode == BUZZ_MODE_OFF)
    {
        _buzz_apply(0, 0);
    }
}

/**
 * @brief 蜂鸣器 10ms 步进函数
 *
 * 需要由上层以固定 10ms 周期调用，
 * 用于推进蜂鸣器节奏状态机。
 */
void bsp_buzz_step_10ms(void)
{
    if (!s_htim)
    {
        return;
    }

    if (s_mode == BUZZ_MODE_OFF)
    {
        /* 关闭模式下保持静音 */
        _buzz_apply(0, 0);
        return;
    }

    uint8_t len = 0;
    const buzz_step_t *pat = _get_pat(s_mode, &len);
    if ((!pat) || (len == 0U))
    {
        _buzz_apply(0, 0);
        return;
    }

    if (s_remain_ms == 0U)
    {
        /* 装载新的节奏步骤 */
        const buzz_step_t *st = &pat[s_idx];
        _buzz_apply(st->on, st->freq_hz);
        s_remain_ms = st->dur_ms;

        /* 切到下一步骤，末尾则回绕 */
        s_idx++;
        if (s_idx >= len)
        {
            s_idx = 0;
        }
    }

    /* 固定 10ms 时基递减剩余时间 */
    if (s_remain_ms >= 10U)
    {
        s_remain_ms -= 10U;
    }
    else
    {
        s_remain_ms = 0U;
    }
}