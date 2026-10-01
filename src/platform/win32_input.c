#include "platform/win32_input.h"

#include <stdint.h>
#include <string.h>

static CfResult send_exact(INPUT *inputs, UINT count)
{
    return SendInput(count, inputs, sizeof(*inputs)) == count
               ? CF_OK
               : CF_ERR_PLATFORM;
}

static DWORD down_flag(CfMouseButton button)
{
    static const DWORD flags[] = {
        MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_RIGHTDOWN
    };
    return button <= CF_MOUSE_RIGHT ? flags[button] : 0;
}

static DWORD up_flag(CfMouseButton button)
{
    static const DWORD flags[] = {
        MOUSEEVENTF_LEFTUP, MOUSEEVENTF_MIDDLEUP, MOUSEEVENTF_RIGHTUP
    };
    return button <= CF_MOUSE_RIGHT ? flags[button] : 0;
}

static CfResult emit_move(const CfAction *action)
{
    int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    INPUT input;

    if (width <= 1 || height <= 1 || action->x < left || action->y < top ||
        action->x >= left + width || action->y >= top + height) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    memset(&input, 0, sizeof(input));
    input.type = INPUT_MOUSE;
    input.mi.dx = (LONG)(((int64_t)action->x - left) * 65535 / (width - 1));
    input.mi.dy = (LONG)(((int64_t)action->y - top) * 65535 / (height - 1));
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE |
                       MOUSEEVENTF_VIRTUALDESK;
    return send_exact(&input, 1);
}

static CfResult win32_emit(void *context, const CfAction *action)
{
    INPUT inputs[2];
    DWORD flag;
    (void)context;

    if (action == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (action->type == CF_ACTION_MOVE) {
        return emit_move(action);
    }
    memset(inputs, 0, sizeof(inputs));
    inputs[0].type = INPUT_MOUSE;
    if (action->type == CF_ACTION_BUTTON_DOWN) {
        flag = down_flag(action->button);
        if (flag == 0) return CF_ERR_INVALID_ARGUMENT;
        inputs[0].mi.dwFlags = flag;
        return send_exact(inputs, 1);
    }
    if (action->type == CF_ACTION_BUTTON_UP) {
        flag = up_flag(action->button);
        if (flag == 0) return CF_ERR_INVALID_ARGUMENT;
        inputs[0].mi.dwFlags = flag;
        return send_exact(inputs, 1);
    }
    if (action->type == CF_ACTION_CLICK) {
        DWORD down = down_flag(action->button);
        DWORD up = up_flag(action->button);
        if (down == 0 || up == 0) return CF_ERR_INVALID_ARGUMENT;
        inputs[0].mi.dwFlags = down;
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = up;
        return send_exact(inputs, 2);
    }
    if (action->type == CF_ACTION_WHEEL) {
        inputs[0].mi.dwFlags = MOUSEEVENTF_WHEEL;
        inputs[0].mi.mouseData = (DWORD)action->wheel_delta;
        return send_exact(inputs, 1);
    }
    return CF_ERR_INVALID_ARGUMENT;
}

static CfWaitResult wait_while_paused(CfWin32Input *input)
{
    HANDLE handles[2] = {input->stop_event, input->resume_event};

    while (WaitForSingleObject(input->pause_event, 0) == WAIT_OBJECT_0) {
        DWORD result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (result == WAIT_OBJECT_0) return CF_WAIT_CANCELLED;
        if (result != WAIT_OBJECT_0 + 1) return CF_WAIT_ERROR;
    }
    return CF_WAIT_ELAPSED;
}

static CfWaitResult win32_wait(void *context, uint32_t milliseconds)
{
    CfWin32Input *input = context;
    uint32_t remaining = milliseconds;

    if (input == NULL || input->timer == NULL) {
        return CF_WAIT_ERROR;
    }
    for (;;) {
        HANDLE handles[3] = {input->stop_event, input->pause_event, input->timer};
        LARGE_INTEGER due;
        uint64_t started;
        DWORD result;
        CfWaitResult pause_result = wait_while_paused(input);

        if (pause_result != CF_WAIT_ELAPSED) return pause_result;
        if (remaining == 0) return CF_WAIT_ELAPSED;
        due.QuadPart = -(LONGLONG)remaining * 10000LL;
        if (!SetWaitableTimer(input->timer, &due, 0, NULL, NULL, FALSE)) {
            return CF_WAIT_ERROR;
        }
        started = GetTickCount64();
        result = WaitForMultipleObjects(3, handles, FALSE, INFINITE);
        if (result == WAIT_OBJECT_0) {
            CancelWaitableTimer(input->timer);
            return CF_WAIT_CANCELLED;
        }
        if (result == WAIT_OBJECT_0 + 2) {
            return CF_WAIT_ELAPSED;
        }
        if (result == WAIT_OBJECT_0 + 1) {
            uint64_t elapsed = GetTickCount64() - started;
            CancelWaitableTimer(input->timer);
            remaining = elapsed >= remaining ? 0 : remaining - (uint32_t)elapsed;
            continue;
        }
        CancelWaitableTimer(input->timer);
        return CF_WAIT_ERROR;
    }
}

static CfResult win32_cursor(void *context, int32_t *x, int32_t *y)
{
    POINT point;
    (void)context;

    if (x == NULL || y == NULL || !GetCursorPos(&point)) {
        return CF_ERR_PLATFORM;
    }
    *x = point.x;
    *y = point.y;
    return CF_OK;
}

static uint64_t win32_clock(void *context)
{
    (void)context;
    return GetTickCount64();
}

CfResult cf_win32_input_init(CfWin32Input *input,
                             HANDLE stop_event,
                             HANDLE pause_event,
                             HANDLE resume_event)
{
    if (input == NULL || stop_event == NULL || pause_event == NULL ||
        resume_event == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    memset(input, 0, sizeof(*input));
    input->stop_event = stop_event;
    input->pause_event = pause_event;
    input->resume_event = resume_event;
    input->timer = CreateWaitableTimerW(NULL, TRUE, NULL);
    return input->timer != NULL ? CF_OK : CF_ERR_PLATFORM;
}

void cf_win32_input_shutdown(CfWin32Input *input)
{
    if (input != NULL && input->timer != NULL) {
        CancelWaitableTimer(input->timer);
        CloseHandle(input->timer);
        input->timer = NULL;
    }
}

CfExecutionOps cf_win32_input_ops(CfWin32Input *input)
{
    CfExecutionOps ops = {0};
    ops.context = input;
    ops.emit = win32_emit;
    ops.wait_ms = win32_wait;
    ops.cursor_position = win32_cursor;
    ops.monotonic_ms = win32_clock;
    return ops;
}
