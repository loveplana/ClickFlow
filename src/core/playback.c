#include "core/playback.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

static bool valid_ops(const CfExecutionOps *ops)
{
    return ops != NULL && ops->emit != NULL && ops->wait_ms != NULL &&
           ops->cursor_position != NULL && ops->monotonic_ms != NULL;
}

static uint32_t scaled_delay(uint32_t milliseconds, double speed)
{
    double scaled = (double)milliseconds / speed;

    if (scaled >= (double)UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)floor(scaled + 0.5);
}

static CfResult wait_for(const CfExecutionOps *ops, uint32_t milliseconds)
{
    CfWaitResult result;

    if (milliseconds == 0) {
        return CF_OK;
    }
    result = ops->wait_ms(ops->context, milliseconds);
    if (result == CF_WAIT_ELAPSED) {
        return CF_OK;
    }
    return result == CF_WAIT_CANCELLED ? CF_ERR_CANCELLED : CF_ERR_PLATFORM;
}

static void release_owned_buttons(const CfExecutionOps *ops,
                                  bool owned[3], CfRunStats *stats)
{
    CfMouseButton button;

    for (button = CF_MOUSE_LEFT; button <= CF_MOUSE_RIGHT;
         button = (CfMouseButton)(button + 1)) {
        if (owned[button]) {
            CfAction release = {0};
            release.type = CF_ACTION_BUTTON_UP;
            release.button = button;
            if (ops->emit(ops->context, &release) == CF_OK && stats != NULL) {
                ++stats->emitted_actions;
            }
            owned[button] = false;
        }
    }
}

CfResult cf_macro_run(const CfMacro *macro,
                      const CfRunOptions *options,
                      const CfExecutionOps *ops,
                      CfRunStats *stats)
{
    CfRunStats local_stats = {0};
    bool owned[3] = {false, false, false};
    uint64_t start;
    uint32_t repeat = 0;
    CfResult result = CF_OK;

    if (macro == NULL || options == NULL || !valid_ops(ops) ||
        (!options->run_forever && options->repeat_count == 0) ||
        (!options->run_forever && options->repeat_count > CF_MACRO_REPEAT_MAX) ||
        !isfinite(options->playback_speed) || options->playback_speed < 0.1 ||
        options->playback_speed > 10.0 ||
        cf_macro_validate(macro, &macro->recorded_screen) != CF_OK) {
        return CF_ERR_INVALID_ARGUMENT;
    }

    start = ops->monotonic_ms(ops->context);
    while (options->run_forever || repeat < options->repeat_count) {
        size_t index;

        for (index = 0; index < macro->actions.count; ++index) {
            const CfAction *action = &macro->actions.items[index];
            uint32_t delay = scaled_delay(action->delay_ms,
                                          options->playback_speed);

            result = wait_for(ops, delay);
            if (result != CF_OK) {
                goto done;
            }
            if (action->type == CF_ACTION_WAIT) {
                continue;
            }
            result = ops->emit(ops->context, action);
            if (result != CF_OK) {
                goto done;
            }
            ++local_stats.emitted_actions;
            if (action->type == CF_ACTION_BUTTON_DOWN) {
                owned[action->button] = true;
            } else if (action->type == CF_ACTION_BUTTON_UP) {
                owned[action->button] = false;
            }
        }
        ++repeat;
        ++local_stats.completed_repeats;
    }

done:
    if (result != CF_OK) {
        release_owned_buttons(ops, owned, &local_stats);
    }
    local_stats.elapsed_ms = ops->monotonic_ms(ops->context) - start;
    if (stats != NULL) {
        *stats = local_stats;
    }
    return result;
}
