#include "runtime/controller.h"

#include "platform/win32_recorder.h"

#include <stdbool.h>
#include <stdlib.h>

typedef enum CfJobType {
    CF_JOB_NONE,
    CF_JOB_CLICKER,
    CF_JOB_MACRO,
    CF_JOB_RECORDING
} CfJobType;

struct CfController {
    CRITICAL_SECTION lock;
    CfTaskState state;
    CfResult last_result;
    CfExecutionOps execution;
    bool execution_ready;
    HANDLE worker;
    HANDLE stop_event;
    HANDLE pause_event;
    HANDLE resume_event;
    HWND notify_window;
    UINT notify_message;
    CfJobType job_type;
    CfClickerConfig clicker;
    CfMacro macro;
    CfRunOptions run_options;
    CfWin32Recorder *recorder;
};

static CfVirtualScreen current_virtual_screen(void)
{
    CfVirtualScreen screen;
    screen.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    screen.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    screen.width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    screen.height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return screen;
}

static void set_state(CfController *controller, CfTaskState state)
{
    EnterCriticalSection(&controller->lock);
    controller->state = state;
    LeaveCriticalSection(&controller->lock);
}

static DWORD WINAPI worker_main(void *context)
{
    CfController *controller = context;
    CfResult result;
    CfRunStats stats;
    CfJobType job_type;

    EnterCriticalSection(&controller->lock);
    job_type = controller->job_type;
    if (controller->state == CF_TASK_STARTING) {
        controller->state = CF_TASK_RUNNING;
    }
    LeaveCriticalSection(&controller->lock);

    if (job_type == CF_JOB_CLICKER) {
        result = cf_clicker_run(&controller->clicker, &controller->execution,
                                &stats);
    } else if (job_type == CF_JOB_MACRO) {
        result = cf_macro_run(&controller->macro, &controller->run_options,
                              &controller->execution, &stats);
    } else {
        result = CF_ERR_PLATFORM;
    }

    EnterCriticalSection(&controller->lock);
    controller->last_result = result == CF_ERR_CANCELLED ? CF_OK : result;
    controller->state = result == CF_OK || result == CF_ERR_CANCELLED
                            ? CF_TASK_IDLE
                            : CF_TASK_ERROR;
    controller->job_type = CF_JOB_NONE;
    LeaveCriticalSection(&controller->lock);

    if (controller->notify_window != NULL && controller->notify_message != 0) {
        PostMessageW(controller->notify_window, controller->notify_message,
                     (WPARAM)result, 0);
    }
    return 0;
}

CfController *cf_controller_create(HWND notify_window, UINT notify_message)
{
    CfController *controller = calloc(1, sizeof(*controller));

    if (controller == NULL) {
        return NULL;
    }
    InitializeCriticalSection(&controller->lock);
    controller->stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    controller->pause_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    controller->resume_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (controller->stop_event == NULL || controller->pause_event == NULL ||
        controller->resume_event == NULL) {
        cf_controller_destroy(controller);
        return NULL;
    }
    controller->state = CF_TASK_IDLE;
    controller->last_result = CF_OK;
    controller->notify_window = notify_window;
    controller->notify_message = notify_message;
    controller->recorder = cf_win32_recorder_create();
    if (controller->recorder == NULL) {
        cf_controller_destroy(controller);
        return NULL;
    }
    cf_macro_init(&controller->macro);
    return controller;
}

static void close_completed_worker(CfController *controller)
{
    if (controller->worker != NULL &&
        WaitForSingleObject(controller->worker, 0) == WAIT_OBJECT_0) {
        CloseHandle(controller->worker);
        controller->worker = NULL;
    }
}

void cf_controller_destroy(CfController *controller)
{
    if (controller == NULL) {
        return;
    }
    cf_controller_shutdown(controller);
    cf_win32_recorder_destroy(controller->recorder);
    cf_macro_free(&controller->macro);
    if (controller->stop_event != NULL) CloseHandle(controller->stop_event);
    if (controller->pause_event != NULL) CloseHandle(controller->pause_event);
    if (controller->resume_event != NULL) CloseHandle(controller->resume_event);
    DeleteCriticalSection(&controller->lock);
    free(controller);
}

CfResult cf_controller_set_execution_ops(CfController *controller,
                                          CfExecutionOps ops)
{
    if (controller == NULL || ops.emit == NULL || ops.wait_ms == NULL ||
        ops.cursor_position == NULL || ops.monotonic_ms == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    EnterCriticalSection(&controller->lock);
    if (controller->state != CF_TASK_IDLE) {
        LeaveCriticalSection(&controller->lock);
        return CF_ERR_CONFLICT;
    }
    controller->execution = ops;
    controller->execution_ready = true;
    LeaveCriticalSection(&controller->lock);
    return CF_OK;
}

static CfResult start_worker(CfController *controller, CfJobType job_type)
{
    close_completed_worker(controller);
    ResetEvent(controller->stop_event);
    ResetEvent(controller->pause_event);
    ResetEvent(controller->resume_event);
    controller->job_type = job_type;
    controller->last_result = CF_OK;
    controller->state = CF_TASK_STARTING;
    controller->worker = CreateThread(NULL, 0, worker_main, controller, 0, NULL);
    if (controller->worker == NULL) {
        controller->state = CF_TASK_ERROR;
        controller->last_result = CF_ERR_PLATFORM;
        controller->job_type = CF_JOB_NONE;
        return CF_ERR_PLATFORM;
    }
    return CF_OK;
}

CfResult cf_controller_start_clicker(CfController *controller,
                                     const CfClickerConfig *config)
{
    CfVirtualScreen screen;
    CfResult result;

    if (controller == NULL || config == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    screen = current_virtual_screen();
    result = cf_clicker_validate(config, &screen);
    if (result != CF_OK) {
        return result;
    }

    EnterCriticalSection(&controller->lock);
    if (controller->state != CF_TASK_IDLE || !controller->execution_ready) {
        LeaveCriticalSection(&controller->lock);
        return CF_ERR_CONFLICT;
    }
    controller->clicker = *config;
    result = start_worker(controller, CF_JOB_CLICKER);
    LeaveCriticalSection(&controller->lock);
    return result;
}

CfResult cf_controller_start_macro(CfController *controller,
                                   const CfMacro *macro,
                                   const CfRunOptions *options)
{
    CfVirtualScreen screen = current_virtual_screen();
    CfResult result;

    if (controller == NULL || macro == NULL || options == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    result = cf_macro_validate(macro, &screen);
    if (result != CF_OK) {
        return result;
    }

    EnterCriticalSection(&controller->lock);
    if (controller->state != CF_TASK_IDLE || !controller->execution_ready) {
        LeaveCriticalSection(&controller->lock);
        return CF_ERR_CONFLICT;
    }
    result = cf_macro_copy(&controller->macro, macro);
    if (result == CF_OK) {
        controller->run_options = *options;
        result = start_worker(controller, CF_JOB_MACRO);
    }
    LeaveCriticalSection(&controller->lock);
    return result;
}

CfResult cf_controller_start_recording(CfController *controller,
                                       const CfVirtualScreen *screen)
{
    CfResult result;

    if (controller == NULL || screen == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    EnterCriticalSection(&controller->lock);
    if (controller->state != CF_TASK_IDLE) {
        LeaveCriticalSection(&controller->lock);
        return CF_ERR_CONFLICT;
    }
    controller->state = CF_TASK_STARTING;
    controller->job_type = CF_JOB_RECORDING;
    LeaveCriticalSection(&controller->lock);

    result = cf_win32_recorder_start(controller->recorder, *screen);
    EnterCriticalSection(&controller->lock);
    controller->last_result = result;
    controller->state = result == CF_OK ? CF_TASK_RUNNING : CF_TASK_ERROR;
    if (result != CF_OK) controller->job_type = CF_JOB_NONE;
    LeaveCriticalSection(&controller->lock);
    return result;
}

CfResult cf_controller_finish_recording(CfController *controller,
                                        const char *name,
                                        CfMacro *out)
{
    CfResult result;

    if (controller == NULL || name == NULL || out == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    result = cf_win32_recorder_finish(controller->recorder, name, out);
    EnterCriticalSection(&controller->lock);
    controller->last_result = result;
    controller->state = result == CF_OK ? CF_TASK_IDLE : CF_TASK_ERROR;
    controller->job_type = CF_JOB_NONE;
    LeaveCriticalSection(&controller->lock);
    return result;
}

CfResult cf_controller_recording_progress(CfController *controller,
                                          size_t *action_count,
                                          uint64_t *elapsed_ms)
{
    if (controller == NULL) return CF_ERR_INVALID_ARGUMENT;
    return cf_win32_recorder_progress(controller->recorder, action_count,
                                      elapsed_ms);
}

CfResult cf_controller_pause(CfController *controller)
{
    if (controller == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    EnterCriticalSection(&controller->lock);
    if (controller->state != CF_TASK_RUNNING) {
        LeaveCriticalSection(&controller->lock);
        return CF_ERR_CONFLICT;
    }
    if (controller->job_type == CF_JOB_RECORDING) {
        CfResult result = cf_win32_recorder_pause(controller->recorder);
        if (result != CF_OK) {
            LeaveCriticalSection(&controller->lock);
            return result;
        }
    } else {
        SetEvent(controller->pause_event);
    }
    controller->state = CF_TASK_PAUSED;
    LeaveCriticalSection(&controller->lock);
    return CF_OK;
}

CfResult cf_controller_resume(CfController *controller)
{
    if (controller == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    EnterCriticalSection(&controller->lock);
    if (controller->state != CF_TASK_PAUSED) {
        LeaveCriticalSection(&controller->lock);
        return CF_ERR_CONFLICT;
    }
    if (controller->job_type == CF_JOB_RECORDING) {
        CfResult result = cf_win32_recorder_resume(controller->recorder);
        if (result != CF_OK) {
            LeaveCriticalSection(&controller->lock);
            return result;
        }
    } else {
        ResetEvent(controller->pause_event);
        SetEvent(controller->resume_event);
    }
    controller->state = CF_TASK_RUNNING;
    LeaveCriticalSection(&controller->lock);
    return CF_OK;
}

CfResult cf_controller_stop(CfController *controller)
{
    if (controller == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    EnterCriticalSection(&controller->lock);
    if (controller->state == CF_TASK_ERROR) {
        controller->state = CF_TASK_IDLE;
        LeaveCriticalSection(&controller->lock);
        return CF_OK;
    }
    if (controller->state == CF_TASK_IDLE) {
        LeaveCriticalSection(&controller->lock);
        return CF_OK;
    }
    if (controller->job_type == CF_JOB_RECORDING) {
        LeaveCriticalSection(&controller->lock);
        cf_win32_recorder_stop(controller->recorder);
        EnterCriticalSection(&controller->lock);
        controller->state = CF_TASK_IDLE;
        controller->job_type = CF_JOB_NONE;
        LeaveCriticalSection(&controller->lock);
        return CF_OK;
    }
    SetEvent(controller->stop_event);
    SetEvent(controller->resume_event);
    controller->state = CF_TASK_STOPPING;
    LeaveCriticalSection(&controller->lock);
    return CF_OK;
}

void cf_controller_shutdown(CfController *controller)
{
    if (controller == NULL) {
        return;
    }
    cf_controller_stop(controller);
    if (controller->worker != NULL) {
        WaitForSingleObject(controller->worker, INFINITE);
        CloseHandle(controller->worker);
        controller->worker = NULL;
    }
    set_state(controller, CF_TASK_IDLE);
}

CfTaskState cf_controller_state(CfController *controller)
{
    CfTaskState state;

    if (controller == NULL) {
        return CF_TASK_ERROR;
    }
    EnterCriticalSection(&controller->lock);
    state = controller->state;
    LeaveCriticalSection(&controller->lock);
    return state;
}

CfResult cf_controller_last_result(CfController *controller)
{
    CfResult result;

    if (controller == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    EnterCriticalSection(&controller->lock);
    result = controller->last_result;
    LeaveCriticalSection(&controller->lock);
    return result;
}

HANDLE cf_controller_stop_event(CfController *controller)
{
    return controller != NULL ? controller->stop_event : NULL;
}

HANDLE cf_controller_pause_event(CfController *controller)
{
    return controller != NULL ? controller->pause_event : NULL;
}

HANDLE cf_controller_resume_event(CfController *controller)
{
    return controller != NULL ? controller->resume_event : NULL;
}
