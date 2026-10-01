#ifndef CLICKFLOW_WIN32_HOTKEY_H
#define CLICKFLOW_WIN32_HOTKEY_H

#include "core/hotkey.h"

#include <windows.h>

CfResult cf_win32_hotkey_register(HWND window, int identifier,
                                  CfHotkey hotkey);
void cf_win32_hotkey_unregister(HWND window, int identifier);

#endif
