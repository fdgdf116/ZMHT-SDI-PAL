#ifndef STREAM_LIFECYCLE_H
#define STREAM_LIFECYCLE_H

#include <pthread.h>
#include <stdint.h>

#include "vivado_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STREAM_STOP_TIMEOUT_MS
#define STREAM_STOP_TIMEOUT_MS 500U
#endif

#ifndef STREAM_READY_TIMEOUT_MS
#define STREAM_READY_TIMEOUT_MS 1000U
#endif

#define STREAM_CHANNEL_MAX DATA_CHANNEL_NUM_MAX
#define STREAM_GLOBAL_CHANNEL (-1)

enum stream_ready_mask {
    STREAM_READY_RGA = 1U << 0,
    STREAM_READY_OSD = 1U << 1,
    STREAM_READY_CROP = 1U << 2,
    STREAM_READY_MPP = 1U << 3,
    STREAM_READY_ALL = STREAM_READY_RGA | STREAM_READY_OSD |
                       STREAM_READY_CROP | STREAM_READY_MPP,
};

typedef enum {
    STREAM_STOPPED = 0,
    STREAM_CONFIGURED,
    STREAM_RUNNING,
    STREAM_STOPPING,
    STREAM_FAULT,
} stream_state_t;

typedef enum {
    STREAM_WORK = 0,
    STREAM_DMA_READ,
    STREAM_DMA_WRITE,
} stream_work_kind_t;

typedef struct {
    int active;
    int chn;
    stream_work_kind_t kind;
} stream_work_guard_t;

stream_state_t stream_state(int chn);
int stream_configure(int chn);
int stream_start(int chn);
void stream_request_stop(int chn);
int stream_finish_stop(int chn);
void stream_set_fault(int chn);
int stream_ready(int chn, unsigned int mask);
int stream_wait_ready(int chn, unsigned int mask, unsigned int timeout_ms);
int stream_wait_idle(int chn, unsigned int timeout_ms);
int stream_join_thread(pthread_t thread, unsigned int timeout_ms);

stream_work_guard_t stream_work_begin(int chn, stream_work_kind_t kind);
void stream_work_end(stream_work_guard_t *guard);

uint64_t stream_monotonic_ms(void);
uint64_t stream_monotonic_us(void);

/* GCC cleanup runs on normal return, continue, break and goto exit. */
#define STREAM_WORK_GUARD(name, chn, kind) \
    stream_work_guard_t name __attribute__((cleanup(stream_work_end))) = \
        stream_work_begin((chn), (kind))

#ifdef __cplusplus
}
#endif

#endif
