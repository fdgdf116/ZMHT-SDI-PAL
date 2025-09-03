/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef _ANDROID_CUTILS_LOGD_H
#define _ANDROID_CUTILS_LOGD_H

/* the stable/frozen log-related definitions have been
 * moved to this header, which is exposed by the NDK
 */

/* the rest is only used internally by the system */
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <pthread.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The userspace structure for version 1 of the logger_entry ABI.
 * This structure is returned to userspace by the kernel logger
 * driver unless an upgrade to a newer ABI version is requested.
 */
struct logger_entry {
    uint16_t    len;    /* length of the payload */
    uint16_t    __pad;  /* no matter what, we get 2 bytes of padding */
    int32_t     pid;    /* generating process's pid */
    int32_t     tid;    /* generating process's tid */
    int32_t     sec;    /* seconds since Epoch */
    int32_t     nsec;   /* nanoseconds */
    char        msg[0]; /* the entry's payload */
};

/*
 * The userspace structure for version 2 of the logger_entry ABI.
 * This structure is returned to userspace if ioctl(LOGGER_SET_VERSION)
 * is called with version==2
 */
struct logger_entry_v2 {
    uint16_t    len;       /* length of the payload */
    uint16_t    hdr_size;  /* sizeof(struct logger_entry_v2) */
    int32_t     pid;       /* generating process's pid */
    int32_t     tid;       /* generating process's tid */
    int32_t     sec;       /* seconds since Epoch */
    int32_t     nsec;      /* nanoseconds */
    uint32_t    euid;      /* effective UID of logger */
    char        msg[0];    /* the entry's payload */
};

#define LOGGER_LOG_MAIN		"log_main"

/*
 * The maximum size of the log entry payload that can be
 * written to the kernel logger driver. An attempt to write
 * more than this amount to /dev/log_main will result in a
 * truncated log entry.
 */
#define LOGGER_ENTRY_MAX_PAYLOAD	4076

/*
 * The maximum size of a log entry which can be read from the
 * kernel logger driver. An attempt to read less than this amount
 * may result in read() returning EINVAL.
 */
#define LOGGER_ENTRY_MAX_LEN		(5*1024)

#define __LOGGERIO	0xAE

#define LOGGER_GET_LOG_BUF_SIZE		    _IO(__LOGGERIO, 1) /* size of log */
#define LOGGER_GET_LOG_LEN				_IO(__LOGGERIO, 2) /* used log len */
#define LOGGER_GET_NEXT_ENTRY_LEN	    _IO(__LOGGERIO, 3) /* next entry len */
#define LOGGER_FLUSH_LOG				_IO(__LOGGERIO, 4) /* flush log */
#define LOGGER_GET_VERSION				_IO(__LOGGERIO, 5) /* abi version */
#define LOGGER_SET_VERSION				_IO(__LOGGERIO, 6) /* abi version */

/*
 * Android log priority values, in ascending priority order.
 */
typedef enum android_LogPriority {
    ANDROID_LOG_UNKNOWN = 0,
    ANDROID_LOG_DEFAULT,    /* only for SetMinPriority() */
    ANDROID_LOG_VERBOSE,
    ANDROID_LOG_DEBUG,
    ANDROID_LOG_INFO,
    ANDROID_LOG_WARN,
    ANDROID_LOG_ERROR,
    ANDROID_LOG_FATAL,
    ANDROID_LOG_SILENT,     /* only for SetMinPriority(); must be last */
} android_LogPriority;

/*
 * Send a simple string to the log.
 */
int __android_log_write(int prio, const char *tag, const char *text);

/*
 * Send a formatted string to the log, used like printf(fmt,...)
 */
int __android_log_print(int prio, const char *tag,  const char *fmt, ...)
//#if defined(__GNUC__)
    __attribute__ ((format(printf, 3, 4)))
//#endif
    ;

/*
 * A variant of __android_log_print() that takes a va_list to list
 * additional parameters.
 */
int __android_log_vprint(int prio, const char *tag,
                         const char *fmt, va_list ap);

/*
 * Log an assertion failure and SIGTRAP the process to have a chance
 * to inspect it, if a debugger is attached. This uses the FATAL priority.
 */
void __android_log_assert(const char *cond, const char *tag,
			  const char *fmt, ...)    
//#if defined(__GNUC__)
    __attribute__ ((noreturn))
    __attribute__ ((format(printf, 3, 4)))
//#endif
    ;


#ifdef __cplusplus
}
#endif

#endif /* _LOGD_H */
