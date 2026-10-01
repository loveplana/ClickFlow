#include "platform/win32_recorder.h"

#include <windows.h>

#include <stdbool.h>
#include <stdlib.h>

struct CfWin32Recorder {
    CRITICAL_SECTION lock;
    HANDLE thread;
    HANDLE stop_event;
    HANDLE ready_event;
    DWORD thread_id;
    HHOOK hook;
    CfRecorderModel model;
    CfResult startup_result;
    CfResult recording_result;
    bool has_model;
    uint64_t started_ms;
    uint64_t pause_started_ms;
    uint64_t paused_total_ms;
};

static CfWin32Recorder *active_recorder;

static bool raw_event_from_message(WPARAM message,
                                   const MSLLHOOKSTRUCT *data,
                                   CfRawMouseEvent *event)
{
    event->timestamp_ms = GetTickCount64();
    event->x = data->pt.x;
    event->y = data->pt.y;
    switch (message) {
    case WM_MOUSEMOVE:
        event->type = CF_RAW_MOVE;
        return true;
    case WM_LBUTTONDOWN:
        event->type = CF_RAW_BUTTON_DOWN;
        event->button = CF_MOUSE_LEFT;
        return true;
    case WM_LBUTTONUP:
        event->type = CF_RAW_BUTTON_UP;
        event->button = CF_MOUSE_LEFT;
        return true;
    case WM_MBUTTONDOWN:
        event->type = CF_RAW_BUTTON_DOWN;
        event->button = CF_MOUSE_MIDDLE;
        return true;
    case WM_MBUTTONUP:
        event->type = CF_RAW_BUTTON_UP;
        event->button = CF_MOUSE_MIDDLE;
        return true;
    case WM_RBUTTONDOWN:
        event->type = CF_RAW_BUTTON_DOWN;
        event->button = CF_MOUSE_RIGHT;
        return true;
    case WM_RBUTTONUP:
        event->type = CF_RAW_BUTTON_UP;
        event->button = CF_MOUSE_RIGHT;
        return true;
    case WM_MOUSEWHEEL:
        event->type = CF_RAW_WHEEL;
        event->wheel_delta = GET_WHEEL_DELTA_WPARAM(data->mouseData);
        return true;
    default:
        return false;
    }
}

static LRESULT CALLBACK mouse_hook(int code, WPARAM message, LPARAM parameter)
{
    if (code == HC_ACTION && active_recorder != NULL) {
        const MSLLHOOKSTRUCT *data = (const MSLLHOOKSTRUCT *)parameter;
        CfRawMouseEvent event = {0};

        if ((data->flags & LLMHF_INJECTED) == 0 &&
            raw_event_from_message(message, data, &event)) {
            EnterCriticalSection(&active_recorder->lock);
            if (active_recorder->has_model) {
                CfResult result = cf_recorder_append(&active_recorder->model,
                                                     &event);
                if (result != CF_OK) {
                    active_recorder->recording_result = result;
                    SetEvent(active_recorder->stop_event);
                }
            }
            LeaveCriticalSection(&active_recorder->lock);
        }
    }
    return CallNextHookEx(NULL, code, message, parameter);
}

static DWORD WINAPI recorder_thread(void *context)
{
    CfWin32Recorder *recorder = context;
    MSG message;

    recorder->thread_id = GetCurrentThreadId();
    PeekMessageW(&message, NULL, WM_USER, WM_USER, PM_NOREMOVE);
    active_recorder = recorder;
    recorder->hook = SetWindowsHookExW(WH_MOUSE_LL, mouse_hook,
                                       GetModuleHandleW(NULL), 0);
    recorder->startup_result =
        recorder->hook != NULL ? CF_OK : CF_ERR_PLATFORM;
    SetEvent(recorder->ready_event);
    if (recorder->hook == NULL) {
        active_recorder = NULL;
        return 0;
    }

    for (;;) {
        DWORD wait = MsgWaitForMultipleObjects(1, &recorder->stop_event, FALSE,
                                               INFINITE, QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0) {
            break;
        }
        if (wait != WAIT_OBJECT_0 + 1) {
            recorder->recording_result = CF_ERR_PLATFORM;
            break;
        }
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                SetEvent(recorder->stop_event);
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    UnhookWindowsHookEx(recorder->hook);
    recorder->hook = NULL;
    active_recorder = NULL;
    return 0;
}

CfWin32Recorder *cf_win32_recorder_create(void)
{
    CfWin32Recorder *recorder = calloc(1, sizeof(*recorder));
    if (recorder == NULL) return NULL;
    InitializeCriticalSection(&recorder->lock);
    recorder->stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    recorder->ready_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (recorder->stop_event == NULL || recorder->ready_event == NULL) {
        cf_win32_recorder_destroy(recorder);
        return NULL;
    }
    return recorder;
}

void cf_win32_recorder_destroy(CfWin32Recorder *recorder)
{
    if (recorder == NULL) return;
    cf_win32_recorder_stop(recorder);
    if (recorder->has_model) cf_recorder_free(&recorder->model);
    if (recorder->stop_event != NULL) CloseHandle(recorder->stop_event);
    if (recorder->ready_event != NULL) CloseHandle(recorder->ready_event);
    DeleteCriticalSection(&recorder->lock);
    free(recorder);
}

CfResult cf_win32_recorder_start(CfWin32Recorder *recorder,
                                 CfVirtualScreen screen)
{
    DWORD ready;

    if (recorder == NULL || recorder->thread != NULL ||
        active_recorder != NULL) {
        return CF_ERR_CONFLICT;
    }
    if (recorder->has_model) {
        cf_recorder_free(&recorder->model);
    }
    recorder->started_ms = GetTickCount64();
    recorder->pause_started_ms = 0;
    recorder->paused_total_ms = 0;
    cf_recorder_init(&recorder->model, screen, recorder->started_ms);
    recorder->has_model = true;
    recorder->startup_result = CF_ERR_PLATFORM;
    recorder->recording_result = CF_OK;
    ResetEvent(recorder->stop_event);
    ResetEvent(recorder->ready_event);
    recorder->thread = CreateThread(NULL, 0, recorder_thread, recorder, 0, NULL);
    if (recorder->thread == NULL) {
        return CF_ERR_PLATFORM;
    }
    ready = WaitForSingleObject(recorder->ready_event, 5000);
    if (ready != WAIT_OBJECT_0 || recorder->startup_result != CF_OK) {
        cf_win32_recorder_stop(recorder);
        return CF_ERR_PLATFORM;
    }
    return CF_OK;
}

CfResult cf_win32_recorder_pause(CfWin32Recorder *recorder)
{
    CfResult result;
    if (recorder == NULL || recorder->thread == NULL) return CF_ERR_CONFLICT;
    EnterCriticalSection(&recorder->lock);
    result = cf_recorder_pause(&recorder->model, GetTickCount64());
    if (result == CF_OK) recorder->pause_started_ms = GetTickCount64();
    LeaveCriticalSection(&recorder->lock);
    return result;
}

CfResult cf_win32_recorder_resume(CfWin32Recorder *recorder)
{
    CfResult result;
    if (recorder == NULL || recorder->thread == NULL) return CF_ERR_CONFLICT;
    EnterCriticalSection(&recorder->lock);
    result = cf_recorder_resume(&recorder->model, GetTickCount64());
    if (result == CF_OK && recorder->pause_started_ms != 0) {
        recorder->paused_total_ms += GetTickCount64() - recorder->pause_started_ms;
        recorder->pause_started_ms = 0;
    }
    LeaveCriticalSection(&recorder->lock);
    return result;
}

CfResult cf_win32_recorder_progress(CfWin32Recorder *recorder,
                                    size_t *action_count,
                                    uint64_t *elapsed_ms)
{
    uint64_t now;
    CfResult result;

    if (recorder == NULL || action_count == NULL || elapsed_ms == NULL ||
        !recorder->has_model) return CF_ERR_INVALID_ARGUMENT;
    EnterCriticalSection(&recorder->lock);
    now = recorder->pause_started_ms != 0 ? recorder->pause_started_ms
                                          : GetTickCount64();
    *action_count = recorder->model.actions.count;
    *elapsed_ms = now >= recorder->started_ms + recorder->paused_total_ms
                      ? now - recorder->started_ms - recorder->paused_total_ms
                      : 0;
    result = recorder->recording_result;
    LeaveCriticalSection(&recorder->lock);
    return result;
}

CfResult cf_win32_recorder_stop(CfWin32Recorder *recorder)
{
    if (recorder == NULL) return CF_ERR_INVALID_ARGUMENT;
    if (recorder->thread == NULL) return CF_OK;
    SetEvent(recorder->stop_event);
    if (recorder->thread_id != 0) PostThreadMessageW(recorder->thread_id, WM_NULL, 0, 0);
    WaitForSingleObject(recorder->thread, INFINITE);
    CloseHandle(recorder->thread);
    recorder->thread = NULL;
    recorder->thread_id = 0;
    return CF_OK;
}

CfResult cf_win32_recorder_finish(CfWin32Recorder *recorder,
                                  const char *name,
                                  CfMacro *out)
{
    CfResult result;
    if (recorder == NULL || !recorder->has_model) return CF_ERR_CONFLICT;
    result = cf_win32_recorder_stop(recorder);
    if (result != CF_OK) return result;
    EnterCriticalSection(&recorder->lock);
    result = recorder->recording_result;
    if (result == CF_OK) {
        result = cf_recorder_finish(&recorder->model, name, out);
    }
    LeaveCriticalSection(&recorder->lock);
    return result;
}
