#include "stream_lifecycle.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t test_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_cv = PTHREAD_COND_INITIALIZER;
static int test_waiting;

static void *in_flight_worker(void *unused)
{
    (void)unused;
    STREAM_WORK_GUARD(work, 0, STREAM_WORK);
    assert(work.active);

    pthread_mutex_lock(&test_lock);
    test_waiting = 1;
    pthread_cond_broadcast(&test_cv);
    while (test_waiting)
        pthread_cond_wait(&test_cv, &test_lock);
    pthread_mutex_unlock(&test_lock);
    return NULL;
}

static void release_worker(void)
{
    pthread_mutex_lock(&test_lock);
    test_waiting = 0;
    pthread_cond_broadcast(&test_cv);
    pthread_mutex_unlock(&test_lock);
}

static void test_stop_during_work(void)
{
    pthread_t thread;

    assert(stream_configure(0) == 0);
    assert(stream_ready(0, STREAM_READY_ALL) == 0);
    assert(stream_wait_ready(0, STREAM_READY_ALL, 10) == 0);
    assert(stream_start(0) == 0);

    assert(pthread_create(&thread, NULL, in_flight_worker, NULL) == 0);
    pthread_mutex_lock(&test_lock);
    while (!test_waiting)
        pthread_cond_wait(&test_cv, &test_lock);
    pthread_mutex_unlock(&test_lock);

    stream_request_stop(0);
    assert(stream_state(0) == STREAM_STOPPING);
    {
        STREAM_WORK_GUARD(rejected, 0, STREAM_WORK);
        assert(!rejected.active);
    }
    assert(stream_start(0) == -EBUSY);
    assert(stream_wait_idle(0, 5) == -ETIMEDOUT);
    assert(stream_join_thread(thread, 5) == -ETIMEDOUT);
    assert(stream_finish_stop(0) == -EBUSY);

    release_worker();
    assert(stream_join_thread(thread, 500) == 0);
    assert(stream_wait_idle(0, 10) == 0);
    assert(stream_finish_stop(0) == 0);
    assert(stream_state(0) == STREAM_STOPPED);
}

static void test_repeated_cycles(void)
{
    for (int i = 0; i < 20; i++) {
        assert(stream_configure(0) == 0);
        assert(stream_start(0) == 0);
        assert(stream_state(0) == STREAM_RUNNING);
        stream_request_stop(0);
        assert(stream_wait_idle(0, 10) == 0);
        assert(stream_finish_stop(0) == 0);
    }
}

int main(void)
{
    test_stop_during_work();
    test_repeated_cycles();

    assert(stream_start(1) == -EBUSY);
    assert(stream_state(99) == STREAM_STOPPED);
    puts("stream lifecycle tests passed");
    return 0;
}
