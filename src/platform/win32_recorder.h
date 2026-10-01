#ifndef CLICKFLOW_WIN32_RECORDER_H
#define CLICKFLOW_WIN32_RECORDER_H

#include "core/recording.h"

typedef struct CfWin32Recorder CfWin32Recorder;

CfWin32Recorder *cf_win32_recorder_create(void);
void cf_win32_recorder_destroy(CfWin32Recorder *recorder);
CfResult cf_win32_recorder_start(CfWin32Recorder *recorder,
                                 CfVirtualScreen screen);
CfResult cf_win32_recorder_pause(CfWin32Recorder *recorder);
CfResult cf_win32_recorder_resume(CfWin32Recorder *recorder);
CfResult cf_win32_recorder_stop(CfWin32Recorder *recorder);
CfResult cf_win32_recorder_finish(CfWin32Recorder *recorder,
                                  const char *name,
                                  CfMacro *out);
CfResult cf_win32_recorder_progress(CfWin32Recorder *recorder,
                                    size_t *action_count,
                                    uint64_t *elapsed_ms);

#endif
