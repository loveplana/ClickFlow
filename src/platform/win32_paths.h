#ifndef CLICKFLOW_WIN32_PATHS_H
#define CLICKFLOW_WIN32_PATHS_H

#include "core/cf_result.h"

#include <stddef.h>
#include <wchar.h>

CfResult cf_win32_data_directory(wchar_t *buffer, size_t buffer_count);
CfResult cf_win32_ensure_data_directories(void);

#endif
