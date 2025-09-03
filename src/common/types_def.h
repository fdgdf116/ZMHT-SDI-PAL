/*
 * File      : types_def.h
 * This file is type def header  
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-1-12     fengyong    first version
 * 2020-4-11     fengyong    second version    
 */

#ifndef _TYPES_DEF_H
#define _TYPES_DEF_H

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <wchar.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#define XF_OK                   0
#define TCP_CONN_NUM            8

typedef enum {    
    RET_OK                      = XF_OK,
    RET_EOK                     =0,
    RET_NOK                     = -1,
    RET_ERR_UNKNOW              = -2,
    RET_ERR_NULL_PTR            = -3,
    RET_ERR_MALLOC              = -4,
    RET_ERR_OPEN_FILE           = -5,
    RET_ERR_VALUE               = -6,
    RET_ERR_READ_BIT            = -7,
    RET_ERR_TIMEOUT             = -8,
    RET_ERR_PERM                = -9,
    RET_ERR_FAILED              = -10,

    RET_ERR_BASE                = -1000,

    /* The error in stream processing */
    RET_ERR_LIST_STREAM         = RET_ERR_BASE - 1,
    RET_ERR_INIT                = RET_ERR_BASE - 2,
    RET_ERR_STREAM              = RET_ERR_BASE - 4,
    RET_ERR_FATAL_THREAD        = RET_ERR_BASE - 5,   
} XF_RET;

#if 0
#define log_debug(format, args...) \
  if (log_get_log_level() <= LOG_LEVEL_DEBUG) printf(format, ##args)
#define log_info(format, args...) \
  if (log_get_log_level() <= LOG_LEVEL_INFO) printf(format, ##args)
#define log_warn(format, args...) \
  if (log_get_log_level() <= LOG_LEVEL_WARN) printf(format, ##args)
#define log_error(format, args...) \
  if (log_get_log_level() <= LOG_LEVEL_ERROR) printf(format, ##args)
#else
#define log_warn(format, args...) printf(format, ##args) 
#endif

#define goto_error_if_fail(p)                           \
  if (!(p)) {                                           \
    log_warn("%s:%d " #p "\n", __FUNCTION__, __LINE__); \
    goto error;                                         \
  }

#define break_if_fail(p)                                \
  if (!(p)) {                                           \
    log_warn("%s:%d " #p "\n", __FUNCTION__, __LINE__); \
    break;                                              \
  }

#define return_if_fail(p)                               \
  if (!(p)) {                                           \
    log_warn("%s:%d " #p "\n", __FUNCTION__, __LINE__); \
    return;                                             \
  }

#define return_value_if_fail(p, value)                  \
  if (!(p)) {                                           \
    log_warn("%s:%d " #p "\n", __FUNCTION__, __LINE__); \
    return (value);                                     \
  }

#endif /* _TYPES_DEF_H */


