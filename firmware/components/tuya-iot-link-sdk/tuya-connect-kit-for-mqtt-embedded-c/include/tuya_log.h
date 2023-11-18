#ifndef __TUYA_LOG_H__
#define __TUYA_LOG_H__

#include "log.h"

#ifdef ENABLE_LOGS
#define TY_LOGE log_error
#define TY_LOGW log_warn
#define TY_LOGI log_info
#define TY_LOGD log_debug
#define TY_LOGV log_trace
#define PR_ERR log_error
#define PR_WARN log_warn
#define PR_INFO log_info
#define PR_DEBUG log_debug
#define PR_TRACE log_trace
#else
#define TY_LOGE(...) ((void)0)
#define TY_LOGW(...) ((void)0)
#define TY_LOGI(...) ((void)0)
#define TY_LOGD(...) ((void)0)
#define TY_LOGV(...) ((void)0)
#define PR_ERR(...) ((void)0)
#define PR_WARN(...) ((void)0)
#define PR_INFO(...) ((void)0)
#define PR_DEBUG(...) ((void)0)
#define PR_TRACE(...) ((void)0)
#endif

#endif
