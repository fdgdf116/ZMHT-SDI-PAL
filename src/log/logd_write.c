/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */ 
#include <time.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "log.h"

enum log_e {		
	LOG_BUF_SIZE = 1024,			
};

#define log_open(pathname, flags) open(pathname, (flags) | O_CLOEXEC)
#define log_writev(filedes, vector, count) writev(filedes, vector, count)
#define log_close(filedes) close(filedes)

static int __write_to_log_init(log_id_t, struct iovec *vec, size_t nr);
static int (*write_to_log)(log_id_t, struct iovec *vec, size_t nr) = __write_to_log_init;

static pthread_mutex_t log_init_lock = PTHREAD_MUTEX_INITIALIZER;

static int log_fds[(int)LOG_ID_MAX] = { -1, -1, -1, -1 };

static unsigned int gs_log_count = 0; 

static struct timeval gs_first_tv; 

static int __write_to_log_null(log_id_t log_fd, struct iovec *vec, size_t nr)
{
    return -1;
}

static int __write_to_log_kernel(log_id_t log_id, struct iovec *vec, size_t nr)
{
    ssize_t ret;
    int log_fd;
	
    if ((int)log_id < (int)LOG_ID_MAX) {
        log_fd = log_fds[(int)log_id];
    } else {
        return EBADF;
    }

    do {	
        ret = log_writev(log_fd, vec, nr);
    } while (ret < 0 && errno == EINTR);

    return ret;
}

static int __write_to_log_init(log_id_t log_id, struct iovec *vec, size_t nr)
{
    pthread_mutex_lock(&log_init_lock);

    if (write_to_log == __write_to_log_init) {
        log_fds[LOG_ID_MAIN] = log_open("/dev/"LOGGER_LOG_MAIN, O_WRONLY);
        
        write_to_log = __write_to_log_kernel;

        if (log_fds[LOG_ID_MAIN] < 0) {           
            write_to_log = __write_to_log_null;
        }        
    }

    pthread_mutex_unlock(&log_init_lock);

    return write_to_log(log_id, vec, nr);
}

static unsigned long diff_timeval(struct timeval *end_tm, struct timeval *start_tm)
{
	struct timeval r;
	int i = 0;
	unsigned long ret = 0;

	r.tv_sec = end_tm->tv_sec - start_tm->tv_sec;
	if (end_tm->tv_usec < start_tm->tv_usec) {
		r.tv_usec = end_tm->tv_usec + 1000000L - start_tm->tv_usec;
		r.tv_sec--;
	} else {
		r.tv_usec = end_tm->tv_usec - start_tm->tv_usec ;
	}

	ret = r.tv_usec;
	for (i = 0; i < (int)(r.tv_sec); i++)
		ret += 1000000L;
	
	return ret;
}

int __android_log_buf_write(int bufID, int prio, const char *tag, const char *msg)
{
    struct iovec vec[3];    
    char msg_buf[LOG_BUF_SIZE] = {0};
    struct timeval tv;
    
    if (0 == gs_log_count) {
        gettimeofday(&gs_first_tv,NULL);
    }
    
    gettimeofday(&tv,NULL);    
    gs_log_count++;
        
    snprintf(msg_buf,sizeof(msg_buf),"[%lu(us)] %s",diff_timeval(&tv,&gs_first_tv),msg);

    if (!tag)
        tag = "";

    vec[0].iov_base   = (unsigned char *) &prio;
    vec[0].iov_len    = 1;
    vec[1].iov_base   = (void *) tag;
    vec[1].iov_len    = strlen(tag) + 1;
    vec[2].iov_base   = (void *) msg_buf;
    vec[2].iov_len    = strlen(msg_buf) + 1;

    return write_to_log((log_id_t)bufID, vec, 3);
}

int __android_log_buf_print(int bufID, int prio, const char *tag, const char *fmt, ...)
{
    va_list ap;
    char buf[LOG_BUF_SIZE];

    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);

    return __android_log_buf_write(bufID, prio, tag, buf);
}

