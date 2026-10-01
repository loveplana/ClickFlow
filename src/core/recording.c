#include "core/recording.h"

#include <stdbool.h>
#include <string.h>

static bool valid_button(CfMouseButton button)
{
    return button == CF_MOUSE_LEFT || button == CF_MOUSE_MIDDLE ||
           button == CF_MOUSE_RIGHT;
}

void cf_recorder_init(CfRecorderModel *recorder,
                      CfVirtualScreen screen,
                      uint64_t started_at_ms)
{
    if (recorder != NULL) {
        cf_action_list_init(&recorder->actions);
        recorder->screen = screen;
        recorder->last_timestamp_ms = started_at_ms;
        recorder->pause_timestamp_ms = 0;
        recorder->paused = false;
        recorder->finished = false;
    }
}

void cf_recorder_free(CfRecorderModel *recorder)
{
    if (recorder != NULL) {
        cf_action_list_free(&recorder->actions);
        memset(recorder, 0, sizeof(*recorder));
    }
}

static CfResult append_action(CfRecorderModel *recorder, CfAction action,
                              uint64_t delay)
{
    if (delay > CF_ACTION_DELAY_MAX_MS) {
        return CF_ERR_LIMIT;
    }
    action.delay_ms = (uint32_t)delay;
    return cf_action_list_push(&recorder->actions, action);
}

CfResult cf_recorder_append(CfRecorderModel *recorder,
                            const CfRawMouseEvent *event)
{
    CfAction action = {0};
    uint64_t delay;
    CfResult result;

    if (recorder == NULL || event == NULL || recorder->finished ||
        event->type < CF_RAW_MOVE || event->type > CF_RAW_WHEEL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (recorder->paused) {
        return CF_ERR_CONFLICT;
    }
    if (event->timestamp_ms < recorder->last_timestamp_ms) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if ((event->type == CF_RAW_BUTTON_DOWN ||
         event->type == CF_RAW_BUTTON_UP) &&
        !valid_button(event->button)) {
        return CF_ERR_INVALID_ARGUMENT;
    }

    delay = event->timestamp_ms - recorder->last_timestamp_ms;
    if (event->type == CF_RAW_MOVE && recorder->actions.count != 0) {
        CfAction *last =
            &recorder->actions.items[recorder->actions.count - 1U];
        if (last->type == CF_ACTION_MOVE &&
            delay <= CF_RECORD_MOVE_COALESCE_MS) {
            if ((uint64_t)last->delay_ms + delay > CF_ACTION_DELAY_MAX_MS) {
                return CF_ERR_LIMIT;
            }
            last->x = event->x;
            last->y = event->y;
            last->delay_ms += (uint32_t)delay;
            recorder->last_timestamp_ms = event->timestamp_ms;
            return CF_OK;
        }
    }

    if (event->type == CF_RAW_BUTTON_UP && recorder->actions.count != 0) {
        CfAction *last =
            &recorder->actions.items[recorder->actions.count - 1U];
        if (last->type == CF_ACTION_BUTTON_DOWN &&
            last->button == event->button && delay <= CF_RECORD_CLICK_MAX_MS) {
            last->type = CF_ACTION_CLICK;
            recorder->last_timestamp_ms = event->timestamp_ms;
            return CF_OK;
        }
    }

    switch (event->type) {
    case CF_RAW_MOVE:
        action.type = CF_ACTION_MOVE;
        action.x = event->x;
        action.y = event->y;
        break;
    case CF_RAW_BUTTON_DOWN:
        action.type = CF_ACTION_BUTTON_DOWN;
        action.button = event->button;
        break;
    case CF_RAW_BUTTON_UP:
        action.type = CF_ACTION_BUTTON_UP;
        action.button = event->button;
        break;
    case CF_RAW_WHEEL:
        action.type = CF_ACTION_WHEEL;
        action.wheel_delta = event->wheel_delta;
        break;
    }

    result = append_action(recorder, action, delay);
    if (result == CF_OK) {
        recorder->last_timestamp_ms = event->timestamp_ms;
    }
    return result;
}

CfResult cf_recorder_pause(CfRecorderModel *recorder, uint64_t timestamp_ms)
{
    if (recorder == NULL || recorder->finished || recorder->paused ||
        timestamp_ms < recorder->last_timestamp_ms) {
        return CF_ERR_CONFLICT;
    }
    recorder->paused = true;
    recorder->pause_timestamp_ms = timestamp_ms;
    return CF_OK;
}

CfResult cf_recorder_resume(CfRecorderModel *recorder, uint64_t timestamp_ms)
{
    if (recorder == NULL || recorder->finished || !recorder->paused ||
        timestamp_ms < recorder->pause_timestamp_ms) {
        return CF_ERR_CONFLICT;
    }
    recorder->paused = false;
    recorder->last_timestamp_ms = timestamp_ms;
    recorder->pause_timestamp_ms = 0;
    return CF_OK;
}

CfResult cf_recorder_finish(CfRecorderModel *recorder,
                            const char *name,
                            CfMacro *out)
{
    CfMacro completed;
    CfResult result;

    if (recorder == NULL || out == NULL || recorder->finished ||
        recorder->actions.count == 0) {
        return CF_ERR_INVALID_ARGUMENT;
    }

    cf_macro_init(&completed);
    result = cf_macro_set_name(&completed, name);
    if (result == CF_OK) {
        result = cf_action_list_copy(&completed.actions, &recorder->actions);
    }
    completed.recorded_screen = recorder->screen;
    if (result == CF_OK) {
        result = cf_macro_validate(&completed, &completed.recorded_screen);
    }
    if (result != CF_OK) {
        cf_macro_free(&completed);
        return result;
    }

    cf_macro_free(out);
    *out = completed;
    recorder->paused = false;
    recorder->finished = true;
    return CF_OK;
}
