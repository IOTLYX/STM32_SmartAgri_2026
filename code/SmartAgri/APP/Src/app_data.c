#include "app_data.h"
#include <string.h>

static app_snapshot_t g_app_snapshot;
static osMutexId_t    g_app_data_mutex = NULL;

static void app_data_lock(void)
{
    if (g_app_data_mutex != NULL)
    {
        (void)osMutexAcquire(g_app_data_mutex, osWaitForever);
    }
}

static void app_data_unlock(void)
{
    if (g_app_data_mutex != NULL)
    {
        (void)osMutexRelease(g_app_data_mutex);
    }
}

void app_data_init(void)
{
    memset(&g_app_snapshot, 0, sizeof(g_app_snapshot));

    if (g_app_data_mutex == NULL)
    {
        g_app_data_mutex = osMutexNew(NULL);
    }
}

void app_data_set_env(const app_env_data_t *env)
{
    if (env == NULL) return;

    app_data_lock();
    g_app_snapshot.env = *env;
    g_app_snapshot.seq++;
    app_data_unlock();
}

void app_data_set_net(const app_net_data_t *net)
{
    if (net == NULL) return;

    app_data_lock();
    g_app_snapshot.net = *net;
    g_app_snapshot.seq++;
    app_data_unlock();
}

void app_data_set_alarm(const app_alarm_data_t *alarm)
{
    if (alarm == NULL) return;

    app_data_lock();
    g_app_snapshot.alarm = *alarm;
    g_app_snapshot.seq++;
    app_data_unlock();
}

void app_data_set_ui_mode(app_ui_mode_t mode)
{
    app_data_lock();
    g_app_snapshot.ui_mode = mode;
    g_app_snapshot.seq++;
    app_data_unlock();
}

void app_data_set_uptime(uint32_t uptime_s)
{
    app_data_lock();
    g_app_snapshot.uptime_s = uptime_s;
    g_app_snapshot.seq++;
    app_data_unlock();
}

void app_data_get_snapshot(app_snapshot_t *out)
{
    if (out == NULL) return;

    app_data_lock();
    *out = g_app_snapshot;
    app_data_unlock();
}

