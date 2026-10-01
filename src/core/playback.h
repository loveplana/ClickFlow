#ifndef CLICKFLOW_PLAYBACK_H
#define CLICKFLOW_PLAYBACK_H

#include "core/action.h"
#include "core/cf_result.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum CfWaitResult {
    CF_WAIT_ELAPSED,
    CF_WAIT_CANCELLED,
    CF_WAIT_ERROR
} CfWaitResult;

typedef struct CfExecutionOps {
    void *context;
    CfResult (*emit)(void *context, const CfAction *action);
    CfWaitResult (*wait_ms)(void *context, uint32_t milliseconds);
    CfResult (*cursor_position)(void *context, int32_t *x, int32_t *y);
    uint64_t (*monotonic_ms)(void *context);
} CfExecutionOps;

typedef struct CfRunOptions {
    bool run_forever;
    uint32_t repeat_count;
    double playback_speed;
} CfRunOptions;

typedef struct CfRunStats {
    uint64_t logical_clicks;
    uint64_t emitted_actions;
    uint64_t completed_repeats;
    uint64_t elapsed_ms;
} CfRunStats;

CfResult cf_macro_run(const CfMacro *macro,
                      const CfRunOptions *options,
                      const CfExecutionOps *ops,
                      CfRunStats *stats);

#endif
