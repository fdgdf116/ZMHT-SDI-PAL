#include "stream_lifecycle.h"

#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    stream_state_t state;
    unsigned int inflight;
    unsigned int ready_mask;
    int configured;
    uint32_t generation;
} stream_channel_info_t;

static pthread_mutex_t g_stream_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_stream_cv = PTHREAD_COND_INITIALIZER;
static stream_channel_info_t g_stream[STREAM_CHANNEL_MAX];

static int stream_valid_channel(int chn)
{
    return chn >= 0 && chn < STREAM_CHANNEL_MAX;
}

uint64_t stream_monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000U + (uint64_t)ts.tv_nsec / 1000000U;
}

uint64_t stream_monotonic_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000U + (uint64_t)ts.tv_nsec / 1000U;
}

static void stream_deadline(unsigned int timeout_ms, struct timespec *deadline)
{
    clock_gettime(CLOCK_MONOTONIC, deadline);
    deadline->tv_sec += timeout_ms / 1000U;
    deadline->tv_nsec += (long)(timeout_ms % 1000U) * 1000000L;
    if (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_sec++;
        deadline->tv_nsec -= 1000000000L;
    }
}

stream_state_t stream_state(int chn)
{
    stream_state_t state;
    if (!stream_valid_channel(chn))
        return STREAM_STOPPED;

    pthread_mutex_lock(&g_stream_lock);
    state = g_stream[chn].state;
    pthread_mutex_unlock(&g_stream_lock);
    return state;
}

int stream_configure(int chn)
{
    if (!stream_valid_channel(chn))
        return -EINVAL;

    pthread_mutex_lock(&g_stream_lock);
    if (g_stream[chn].state == STREAM_RUNNING) {
        pthread_mutex_unlock(&g_stream_lock);
        return -EBUSY;
    }
    if (g_stream[chn].state == STREAM_STOPPING) {
        pthread_mutex_unlock(&g_stream_lock);
        return -EAGAIN;
    }

    g_stream[chn].state = STREAM_CONFIGURED;
    g_stream[chn].configured = 1;
    g_stream[chn].ready_mask = 0;
    g_stream[chn].generation++;
    pthread_cond_broadcast(&g_stream_cv);
    pthread_mutex_unlock(&g_stream_lock);
    return 0;
}

int stream_ready(int chn, unsigned int mask)
{
    if (!stream_valid_channel(chn) || (mask & ~STREAM_READY_ALL))
        return -EINVAL;

    pthread_mutex_lock(&g_stream_lock);
    g_stream[chn].ready_mask |= mask;
    pthread_cond_broadcast(&g_stream_cv);
    pthread_mutex_unlock(&g_stream_lock);
    return 0;
}

int stream_wait_ready(int chn, unsigned int mask, unsigned int timeout_ms)
{
    struct timespec deadline;
    int ret = 0;

    if (!stream_valid_channel(chn) || (mask & ~STREAM_READY_ALL))
        return -EINVAL;

    stream_deadline(timeout_ms, &deadline);
    pthread_mutex_lock(&g_stream_lock);
    while ((g_stream[chn].ready_mask & mask) != mask) {
        if (g_stream[chn].state == STREAM_FAULT) {
            ret = -EIO;
            break;
        }
        if (g_stream[chn].state != STREAM_CONFIGURED &&
            g_stream[chn].state != STREAM_RUNNING) {
            ret = -EINVAL;
            break;
        }
        ret = pthread_cond_timedwait(&g_stream_cv, &g_stream_lock, &deadline);
        if (ret == ETIMEDOUT) {
            ret = -ETIMEDOUT;
            break;
        }
    }
    pthread_mutex_unlock(&g_stream_lock);
    return ret;
}

int stream_start(int chn)
{
    int ret = 0;
    if (!stream_valid_channel(chn))
        return -EINVAL;

    pthread_mutex_lock(&g_stream_lock);
    if (g_stream[chn].state == STREAM_RUNNING) {
        ret = 0;
    } else if (g_stream[chn].state != STREAM_CONFIGURED ||
               !g_stream[chn].configured ||
               g_stream[chn].inflight != 0) {
        ret = -EBUSY;
    } else {
        g_stream[chn].state = STREAM_RUNNING;
        pthread_cond_broadcast(&g_stream_cv);
    }
    pthread_mutex_unlock(&g_stream_lock);
    return ret;
}

void stream_request_stop(int chn)
{
    if (!stream_valid_channel(chn))
        return;

    pthread_mutex_lock(&g_stream_lock);
    if (g_stream[chn].state != STREAM_FAULT)
        g_stream[chn].state = STREAM_STOPPING;
    pthread_cond_broadcast(&g_stream_cv);
    pthread_mutex_unlock(&g_stream_lock);
}

int stream_finish_stop(int chn)
{
    int ret = -EBUSY;
    if (!stream_valid_channel(chn))
        return -EINVAL;

    pthread_mutex_lock(&g_stream_lock);
    if (g_stream[chn].state == STREAM_STOPPING &&
        g_stream[chn].inflight == 0) {
        g_stream[chn].state = STREAM_STOPPED;
        g_stream[chn].configured = 0;
        g_stream[chn].ready_mask = 0;
        ret = 0;
    }
    pthread_cond_broadcast(&g_stream_cv);
    pthread_mutex_unlock(&g_stream_lock);
    return ret;
}

void stream_set_fault(int chn)
{
    if (!stream_valid_channel(chn))
        return;

    pthread_mutex_lock(&g_stream_lock);
    g_stream[chn].state = STREAM_FAULT;
    pthread_cond_broadcast(&g_stream_cv);
    pthread_mutex_unlock(&g_stream_lock);
}

stream_work_guard_t stream_work_begin(int chn, stream_work_kind_t kind)
{
    stream_work_guard_t guard = {0, chn, kind};
    if (!stream_valid_channel(chn))
        return guard;

    pthread_mutex_lock(&g_stream_lock);
    if (g_stream[chn].state == STREAM_RUNNING) {
        guard.active = 1;
        g_stream[chn].inflight++;
    }
    pthread_mutex_unlock(&g_stream_lock);
    return guard;
}

void stream_work_end(stream_work_guard_t *guard)
{
    if (!guard || !guard->active)
        return;

    pthread_mutex_lock(&g_stream_lock);
    if (stream_valid_channel(guard->chn) && g_stream[guard->chn].inflight)
        g_stream[guard->chn].inflight--;
    if (stream_valid_channel(guard->chn) && !g_stream[guard->chn].inflight)
        pthread_cond_broadcast(&g_stream_cv);
    guard->active = 0;
    pthread_mutex_unlock(&g_stream_lock);
}

int stream_wait_idle(int chn, unsigned int timeout_ms)
{
    struct timespec deadline;
    int ret = 0;

    if (!stream_valid_channel(chn))
        return -EINVAL;

    stream_deadline(timeout_ms, &deadline);
    pthread_mutex_lock(&g_stream_lock);
    while (g_stream[chn].inflight != 0) {
        ret = pthread_cond_timedwait(&g_stream_cv, &g_stream_lock, &deadline);
        if (ret == ETIMEDOUT) {
            ret = -ETIMEDOUT;
            break;
        }
    }
    pthread_mutex_unlock(&g_stream_lock);
    return ret;
}

int stream_join_thread(pthread_t thread, unsigned int timeout_ms)
{
    struct timespec deadline;
    int ret;

    stream_deadline(timeout_ms, &deadline);
    while ((ret = pthread_tryjoin_np(thread, NULL)) == EBUSY) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))
            return -ETIMEDOUT;
        usleep(500);
    }
    return -ret;
}
