#ifndef CLICKFLOW_CONTROLLER_H
#define CLICKFLOW_CONTROLLER_H

#include "core/clicker.h"
#include "core/playback.h"
#include "core/recording.h"

#include <windows.h>

typedef enum CfTaskState {
    CF_TASK_IDLE,
    CF_TASK_STARTING,
    CF_TASK_RUNNING,
    CF_TASK_PAUSED,
    CF_TASK_STOPPING,
    CF_TASK_ERROR
} CfTaskState;

typedef struct CfController CfController;

CfController *cf_controller_create(HWND notify_window, UINT notify_message);
void cf_controller_destroy(CfController *controller);
CfResult cf_controller_set_execution_ops(CfController *controller,
                                          CfExecutionOps ops);
CfResult cf_controller_start_clicker(CfController *controller,
                                     const CfClickerConfig *config);
CfResult cf_controller_start_macro(CfController *controller,
                                   const CfMacro *macro,
                                   const CfRunOptions *options);
CfResult cf_controller_start_recording(CfController *controller,
                                       const CfVirtualScreen *screen);
CfResult cf_controller_finish_recording(CfController *controller,
                                        const char *name,
                                        CfMacro *out);
CfResult cf_controller_recording_progress(CfController *controller,
                                          size_t *action_count,
                                          uint64_t *elapsed_ms);
CfResult cf_controller_pause(CfController *controller);
CfResult cf_controller_resume(CfController *controller);
CfResult cf_controller_stop(CfController *controller);
void cf_controller_shutdown(CfController *controller);
CfTaskState cf_controller_state(CfController *controller);
CfResult cf_controller_last_result(CfController *controller);
HANDLE cf_controller_stop_event(CfController *controller);
HANDLE cf_controller_pause_event(CfController *controller);
HANDLE cf_controller_resume_event(CfController *controller);

#endif
