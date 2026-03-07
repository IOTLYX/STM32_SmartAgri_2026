#include "bsp_buzz.h"

/* ====== 你可以按需要微调这些参数 ====== */
#define BUZZ_FREQ_HIGH_HZ      2500u
#define BUZZ_FREQ_LOW_HZ       1200u
#define BUZZ_FREQ_RAIN_HZ      2000u
#define BUZZ_FREQ_NET_HZ       1500u

/* TIM3: 72MHz / (PSC+1=72) = 1MHz 计数 */
#define BUZZ_TMR_CNT_HZ        1000000u

typedef struct {
    uint16_t dur_ms;     // 本步骤持续时间
    uint16_t freq_hz;    // 0=静音，否则设置频率
    uint8_t  on;         // 1=响，0=不响（不响时freq忽略）
} buzz_step_t;

static TIM_HandleTypeDef *s_htim = NULL;
static uint32_t s_ch = 0;
static buzz_mode_t s_mode = BUZZ_MODE_OFF;

/* 步进状态 */
static uint8_t s_idx = 0;
static uint16_t s_remain_ms = 0;

/* ====== 内部：设置频率 + 占空比 ====== */
static void _buzz_apply(uint8_t on, uint16_t freq_hz)
{
    if (!s_htim) return;

    if (!on || freq_hz == 0) {
        __HAL_TIM_SET_COMPARE(s_htim, s_ch, 0); // 静音
        return;
    }

    /* ARR = (timer_clk / freq) - 1 */
    uint32_t arr = (BUZZ_TMR_CNT_HZ / (uint32_t)freq_hz);
    if (arr == 0) arr = 1;
    arr -= 1;
    if (arr > 0xFFFFu) arr = 0xFFFFu;

    __HAL_TIM_SET_AUTORELOAD(s_htim, (uint16_t)arr);
    __HAL_TIM_SET_COUNTER(s_htim, 0);

    /* 50% 占空比 */
    uint16_t ccr = (uint16_t)((arr + 1u) / 2u);
    __HAL_TIM_SET_COMPARE(s_htim, s_ch, ccr);
}

/* ====== 三种节奏表 ======
 * 说明：
 * - 缺水：高频响-停-低频响-停 循环（“高频然后低频快速轮流响”）
 * - 降雨：嘀嘀嘀---嘀嘀嘀---（3短 + 长停 循环）
 * - 断网：滴-----滴-----（短响 + 长停 循环）
 */
static const buzz_step_t s_pat_dry[] = {
    {120, BUZZ_FREQ_HIGH_HZ, 1},
    { 60, 0,                0},
    {120, BUZZ_FREQ_LOW_HZ,  1},
    { 60, 0,                0},
};

static const buzz_step_t s_pat_rain[] = {
    {80, BUZZ_FREQ_RAIN_HZ, 1},
    {80, 0,                0},
    {80, BUZZ_FREQ_RAIN_HZ, 1},
    {80, 0,                0},
    {80, BUZZ_FREQ_RAIN_HZ, 1},
    {400,0,                0},   // “---”
};

static const buzz_step_t s_pat_net[] = {
    {200, BUZZ_FREQ_NET_HZ, 1},
    {800, 0,                0},
};

static const buzz_step_t* _get_pat(buzz_mode_t m, uint8_t *len)
{
    switch (m) {
    case BUZZ_MODE_DRY:
        *len = (uint8_t)(sizeof(s_pat_dry)/sizeof(s_pat_dry[0]));
        return s_pat_dry;
    case BUZZ_MODE_RAIN:
        *len = (uint8_t)(sizeof(s_pat_rain)/sizeof(s_pat_rain[0]));
        return s_pat_rain;
    case BUZZ_MODE_NET_LOST:
        *len = (uint8_t)(sizeof(s_pat_net)/sizeof(s_pat_net[0]));
        return s_pat_net;
    default:
        *len = 0;
        return NULL;
    }
}

void bsp_buzz_init(TIM_HandleTypeDef *htim, uint32_t channel)
{
    s_htim = htim;
    s_ch = channel;
    s_mode = BUZZ_MODE_OFF;
    s_idx = 0;
    s_remain_ms = 0;

    if (s_htim) {
        HAL_TIM_PWM_Start(s_htim, s_ch);
        _buzz_apply(0, 0);
    }
}

void bsp_buzz_off(void)
{
    s_mode = BUZZ_MODE_OFF;
    s_idx = 0;
    s_remain_ms = 0;
    _buzz_apply(0, 0);
}

void bsp_buzz_set_mode(buzz_mode_t mode)
{
    if (s_mode == mode) return;

    s_mode = mode;
    s_idx = 0;
    s_remain_ms = 0;

    if (s_mode == BUZZ_MODE_OFF) {
        _buzz_apply(0, 0);
    }
}

void bsp_buzz_step_10ms(void)
{
    if (!s_htim) return;

    if (s_mode == BUZZ_MODE_OFF) {
        /* 保持静音 */
        _buzz_apply(0, 0);
        return;
    }

    uint8_t len = 0;
    const buzz_step_t *pat = _get_pat(s_mode, &len);
    if (!pat || len == 0) {
        _buzz_apply(0, 0);
        return;
    }

    if (s_remain_ms == 0) {
        /* 装载新步骤 */
        const buzz_step_t *st = &pat[s_idx];
        _buzz_apply(st->on, st->freq_hz);
        s_remain_ms = st->dur_ms;

        s_idx++;
        if (s_idx >= len) s_idx = 0;
    }

    /* 10ms tick */
    if (s_remain_ms >= 10) s_remain_ms -= 10;
    else s_remain_ms = 0;
}
