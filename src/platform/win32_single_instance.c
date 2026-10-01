#include "platform/win32_single_instance.h"

#include <string.h>

CfResult cf_single_instance_acquire(CfSingleInstance *instance)
{
    if (instance == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    memset(instance, 0, sizeof(*instance));
    instance->mutex = CreateMutexW(NULL, FALSE,
                                   L"Local\\ClickFlow-1E9845BC-269B-4A54");
    if (instance->mutex == NULL) {
        return CF_ERR_PLATFORM;
    }
    instance->primary = GetLastError() != ERROR_ALREADY_EXISTS;
    return instance->primary ? CF_OK : CF_ERR_CONFLICT;
}

void cf_single_instance_activate_existing(const wchar_t *window_class)
{
    HWND window = FindWindowW(window_class, NULL);
    if (window != NULL) {
        if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
        SetForegroundWindow(window);
    }
}

void cf_single_instance_release(CfSingleInstance *instance)
{
    if (instance != NULL && instance->mutex != NULL) {
        CloseHandle(instance->mutex);
        instance->mutex = NULL;
        instance->primary = false;
    }
}
