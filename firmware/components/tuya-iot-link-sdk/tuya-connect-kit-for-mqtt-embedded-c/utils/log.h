/**
 * Copyright (c) 2020 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"

#define LOG_VERSION "0.1.0"
#define LOG_USE_COLOR

typedef struct
{
  va_list ap;
  const char *fmt;
  const char *file;
  struct tm *time;
  void *udata;
  int line;
  int level;
} log_Event;

typedef void (*log_LogFn)(log_Event *ev);
typedef void (*log_LockFn)(bool lock, void *udata);

enum
{
  LOG_TRACE,
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
  LOG_FATAL
};

#define __FILENAME_TUYA__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// #define TUYA_DEBUG_LOGS

#ifdef TUYA_DEBUG_LOGS

#define log_trace(...) log_log(LOG_TRACE, __FILENAME_TUYA__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __FILENAME_TUYA__, __LINE__, __VA_ARGS__)
#define log_info(...) log_log(LOG_INFO, __FILENAME_TUYA__, __LINE__, __VA_ARGS__)
#define log_warn(...) log_log(LOG_WARN, __FILENAME_TUYA__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __FILENAME_TUYA__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_FATAL, __FILENAME_TUYA__, __LINE__, __VA_ARGS__)

#else

#define log_trace(...) ESP_LOGV("TUYA", __VA_ARGS__)
#define log_debug(...) ESP_LOGD("TUYA", __VA_ARGS__)
#define log_info(...) ESP_LOGI("TUYA", __VA_ARGS__)
#define log_warn(...) ESP_LOGW("TUYA", __VA_ARGS__)
#define log_error(...) ESP_LOGE("TUYA", __VA_ARGS__)
#define log_fatal(...) ESP_LOGE("TUYA", __VA_ARGS__)

#endif

const char *log_level_string(int level);
void log_set_lock(log_LockFn fn, void *udata);
void log_set_level(int level);
void log_set_quiet(bool enable);
int log_add_callback(log_LogFn fn, void *udata, int level);
int log_add_fp(FILE *fp, int level);

void log_log(int level, const char *file, int line, const char *fmt, ...);

void base_log_init();

#endif
