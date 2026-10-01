#ifndef CLICKFLOW_RECORDING_H
#define CLICKFLOW_RECORDING_H

#include "core/action.h"
#include "core/cf_result.h"

#include <stdbool.h>
#include <stdint.h>

#define CF_RECORD_MOVE_COALESCE_MS 8U
#define CF_RECORD_CLICK_MAX_MS 250U

typedef enum CfRawMouseEventType {
    CF_RAW_MOVE,
    CF_RAW_BUTTON_DOWN,
    CF_RAW_BUTTON_UP,
    CF_RAW_WHEEL
} CfRawMouseEventType;

typedef struct CfRawMouseEvent {
    CfRawMouseEventType type;
    uint64_t timestamp_ms;
    int32_t x;
    int32_t y;
    int32_t wheel_delta;
    CfMouseButton button;
} CfRawMouseEvent;

typedef struct CfRecorderModel {
    CfActionList actions;
    CfVirtualScreen screen;
    uint64_t last_timestamp_ms;
    uint64_t pause_timestamp_ms;
    bool paused;
    bool finished;
} CfRecorderModel;

void cf_recorder_init(CfRecorderModel *recorder,
                      CfVirtualScreen screen,
                      uint64_t started_at_ms);
void cf_recorder_free(CfRecorderModel *recorder);
CfResult cf_recorder_append(CfRecorderModel *recorder,
                            const CfRawMouseEvent *event);
CfResult cf_recorder_pause(CfRecorderModel *recorder, uint64_t timestamp_ms);
CfResult cf_recorder_resume(CfRecorderModel *recorder, uint64_t timestamp_ms);
CfResult cf_recorder_finish(CfRecorderModel *recorder,
                            const char *name,
                            CfMacro *out);

#endif
