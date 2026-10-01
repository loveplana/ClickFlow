#ifndef CLICKFLOW_WIN32_SINGLE_INSTANCE_H
#define CLICKFLOW_WIN32_SINGLE_INSTANCE_H

#include "core/cf_result.h"

#include <stdbool.h>
#include <windows.h>

typedef struct CfSingleInstance {
    HANDLE mutex;
    bool primary;
} CfSingleInstance;

CfResult cf_single_instance_acquire(CfSingleInstance *instance);
void cf_single_instance_activate_existing(const wchar_t *window_class);
void cf_single_instance_release(CfSingleInstance *instance);

#endif
