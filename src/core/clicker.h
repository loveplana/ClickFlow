#ifndef CLICKFLOW_CLICKER_H
#define CLICKFLOW_CLICKER_H

#include "core/action.h"
#include "core/cf_result.h"
#include "core/playback.h"

#include <stdint.h>

#define CF_CLICK_INTERVAL_MAX_MS 2592000000U
#define CF_CLICK_COUNT_MAX 1000000000ULL
#define CF_CLICK_DURATION_MAX_MS 2592000000U

typedef enum CfClickKind {
    CF_CLICK_SINGLE,
    CF_CLICK_DOUBLE
} CfClickKind;

typedef enum CfStopMode {
    CF_STOP_MANUAL,
    CF_STOP_AFTER_COUNT,
    CF_STOP_AFTER_DURATION
} CfStopMode;

typedef enum CfPositionMode {
    CF_POSITION_CURSOR,
    CF_POSITION_FIXED
} CfPositionMode;

typedef struct CfClickerConfig {
    CfMouseButton button;
    CfClickKind click_kind;
    CfStopMode stop_mode;
    CfPositionMode position_mode;
    uint32_t interval_ms;
    uint64_t click_count;
    uint32_t duration_ms;
    int32_t fixed_x;
    int32_t fixed_y;
} CfClickerConfig;

CfResult cf_clicker_validate(const CfClickerConfig *config,
                             const CfVirtualScreen *screen);
CfResult cf_clicker_run(const CfClickerConfig *config,
                        const CfExecutionOps *ops,
                        CfRunStats *stats);

#endif
