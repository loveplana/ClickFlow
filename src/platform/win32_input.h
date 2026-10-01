#ifndef CLICKFLOW_WIN32_INPUT_H
#define CLICKFLOW_WIN32_INPUT_H

#include "core/playback.h"

#include <windows.h>

typedef struct CfWin32Input {
    HANDLE stop_event;
    HANDLE pause_event;
    HANDLE resume_event;
    HANDLE timer;
} CfWin32Input;

CfResult cf_win32_input_init(CfWin32Input *input,
                             HANDLE stop_event,
                             HANDLE pause_event,
                             HANDLE resume_event);
void cf_win32_input_shutdown(CfWin32Input *input);
CfExecutionOps cf_win32_input_ops(CfWin32Input *input);

#endif
