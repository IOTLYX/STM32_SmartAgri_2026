#include "app_data.h"
#include <string.h>

/* 全局应用快照数据，集中保存环境、网络、告警、UI、运行时间等状态 */
static app_snapshot_t g_app_snapshot;

/* 应用数据互斥锁，用于保护多任务并发访问共享快照数据 */
static osMutexId_t    g_app_data_mutex = NULL;

/**
 * @brief 加锁应用数据区
 *
 * 当互斥锁已创建时，阻塞等待直到成功获取，
 * 用于保护全局快照数据的读写原子性。
 */
static void app_data_lock(void)
{
    if (g_app_data_mutex != NULL)
    {
        /* 等待直到获取互斥锁，防止多任务同时访问共享数据 */
        (void)osMutexAcquire(g_app_data_mutex, osWaitForever);
    }
}

/**
 * @brief 解锁应用数据区
 *
 * 当互斥锁已创建时，释放当前持有的互斥锁，
 * 允许其他任务继续访问全局快照数据。
 */
static void app_data_unlock(void)
{
    if (g_app_data_mutex != NULL)
    {
        /* 释放互斥锁，结束本次共享数据访问保护 */
        (void)osMutexRelease(g_app_data_mutex);
    }
}

/**
 * @brief 初始化应用数据模块
 *
 * 清零全局快照数据，并创建互斥锁，
 * 为后续多任务安全访问共享数据做好准备。
 */
void app_data_init(void)
{
    /* 将整个快照结构清零，建立默认初始状态 */
    memset(&g_app_snapshot, 0, sizeof(g_app_snapshot));

    /* 互斥锁仅创建一次，避免重复分配系统资源 */
    if (g_app_data_mutex == NULL)
    {
        g_app_data_mutex = osMutexNew(NULL);
    }
}

/**
 * @brief 更新环境数据
 *
 * 将外部传入的环境数据写入全局快照，
 * 并递增数据序号，便于其他模块判断数据是否发生变化。
 *
 * @param[in] env 指向新的环境数据结构体
 */
void app_data_set_env(const app_env_data_t *env)
{
    if (env == NULL) return;

    app_data_lock();

    /* 整体覆盖环境数据，保持结构体赋值简洁高效 */
    g_app_snapshot.env = *env;

    /* 每次数据更新后递增序号，便于快照一致性判断 */
    g_app_snapshot.seq++;

    app_data_unlock();
}

/**
 * @brief 更新网络状态数据
 *
 * 将外部传入的网络状态写入全局快照，
 * 并递增数据序号。
 *
 * @param[in] net 指向新的网络状态数据结构体
 */
void app_data_set_net(const app_net_data_t *net)
{
    if (net == NULL) return;

    app_data_lock();

    /* 更新网络状态快照 */
    g_app_snapshot.net = *net;

    /* 标记全局数据已更新 */
    g_app_snapshot.seq++;

    app_data_unlock();
}

/**
 * @brief 更新告警状态数据
 *
 * 将外部传入的告警状态写入全局快照，
 * 并递增数据序号。
 *
 * @param[in] alarm 指向新的告警状态数据结构体
 */
void app_data_set_alarm(const app_alarm_data_t *alarm)
{
    if (alarm == NULL) return;

    app_data_lock();

    /* 更新告警状态快照 */
    g_app_snapshot.alarm = *alarm;

    /* 标记全局数据已更新 */
    g_app_snapshot.seq++;

    app_data_unlock();
}

/**
 * @brief 更新当前 UI 模式
 *
 * 用于同步当前界面模式到全局快照，
 * 供 UI 任务或其他模块查询。
 *
 * @param[in] mode 当前 UI 模式
 */
void app_data_set_ui_mode(app_ui_mode_t mode)
{
    app_data_lock();

    /* 更新当前界面模式 */
    g_app_snapshot.ui_mode = mode;

    /* 标记全局数据已更新 */
    g_app_snapshot.seq++;

    app_data_unlock();
}

/**
 * @brief 更新系统运行时间
 *
 * 将当前运行秒数同步到全局快照，
 * 便于显示模块或网络上报模块读取。
 *
 * @param[in] uptime_s 系统已运行时间，单位 s
 */
void app_data_set_uptime(uint32_t uptime_s)
{
    app_data_lock();

    /* 更新系统运行时间 */
    g_app_snapshot.uptime_s = uptime_s;

    /* 标记全局数据已更新 */
    g_app_snapshot.seq++;

    app_data_unlock();
}

/**
 * @brief 获取当前应用数据快照
 *
 * 在互斥保护下，将全局快照完整复制到输出对象，
 * 以保证读取过程中数据一致。
 *
 * @param[out] out 用于接收快照数据的输出结构体指针
 */
void app_data_get_snapshot(app_snapshot_t *out)
{
    if (out == NULL) return;

    app_data_lock();

    /* 将当前完整快照复制给调用方，避免逐字段读取不一致 */
    *out = g_app_snapshot;

    app_data_unlock();
}