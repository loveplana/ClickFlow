#include "core/clicker.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static bool valid_button(CfMouseButton button)
{
    return button == CF_MOUSE_LEFT || button == CF_MOUSE_MIDDLE ||
           button == CF_MOUSE_RIGHT;
}

static bool valid_screen(const CfVirtualScreen *screen)
{
    int64_t right;
    int64_t bottom;

    if (screen == NULL || screen->width <= 0 || screen->height <= 0) {
        return false;
    }
    right = (int64_t)screen->left + screen->width;
    bottom = (int64_t)screen->top + screen->height;
    return right <= INT32_MAX && right >= INT32_MIN &&
           bottom <= INT32_MAX && bottom >= INT32_MIN;
}

static bool point_on_screen(int32_t x, int32_t y,
                            const CfVirtualScreen *screen)
{
    int64_t right = (int64_t)screen->left + screen->width;
    int64_t bottom = (int64_t)screen->top + screen->height;

    return x >= screen->left && (int64_t)x < right && y >= screen->top &&
           (int64_t)y < bottom;
}

CfResult cf_clicker_validate(const CfClickerConfig *config,
                             const CfVirtualScreen *screen)
{
    if (config == NULL || !valid_screen(screen) ||
        !valid_button(config->button) ||
        (config->click_kind != CF_CLICK_SINGLE &&
         config->click_kind != CF_CLICK_DOUBLE) ||
        config->stop_mode < CF_STOP_MANUAL ||
        config->stop_mode > CF_STOP_AFTER_DURATION ||
        (config->position_mode != CF_POSITION_CURSOR &&
         config->position_mode != CF_POSITION_FIXED) ||
        config->interval_ms == 0 ||
        config->interval_ms > CF_CLICK_INTERVAL_MAX_MS) {
        return CF_ERR_INVALID_ARGUMENT;
    }

    if (config->stop_mode == CF_STOP_AFTER_COUNT &&
        (config->click_count == 0 ||
         config->click_count > CF_CLICK_COUNT_MAX)) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (config->stop_mode == CF_STOP_AFTER_DURATION &&
        (config->duration_ms == 0 ||
         config->duration_ms > CF_CLICK_DURATION_MAX_MS)) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (config->position_mode == CF_POSITION_FIXED &&
        !point_on_screen(config->fixed_x, config->fixed_y, screen)) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    return CF_OK;
}

static bool valid_ops(const CfExecutionOps *ops)
{
    return ops != NULL && ops->emit != NULL && ops->wait_ms != NULL &&
           ops->cursor_position != NULL && ops->monotonic_ms != NULL;
}

static CfResult emit_action(const CfExecutionOps *ops, CfAction action,
                            CfRunStats *stats)
{
    CfResult result = ops->emit(ops->context, &action);
    if (result == CF_OK) {
        ++stats->emitted_actions;
    }
    return result;
}

CfResult cf_clicker_run(const CfClickerConfig *config,
                        const CfExecutionOps *ops,
                        CfRunStats *stats)
{
    CfRunStats local_stats;
    CfVirtualScreen validation;
    uint64_t start;
    CfResult result = CF_OK;

    if (config == NULL || !valid_ops(ops)) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    validation = config->position_mode == CF_POSITION_FIXED
                     ? (CfVirtualScreen){config->fixed_x, config->fixed_y, 1, 1}
                     : (CfVirtualScreen){0, 0, 1, 1};
    if (cf_clicker_validate(config, &validation) != CF_OK) {
        return CF_ERR_INVALID_ARGUMENT;
    }

    memset(&local_stats, 0, sizeof(local_stats));
    start = ops->monotonic_ms(ops->context);
    for (;;) {
        uint64_t elapsed = ops->monotonic_ms(ops->context) - start;
        CfAction click = {0};
        unsigned click_index;
        unsigned clicks_per_cycle =
            config->click_kind == CF_CLICK_DOUBLE ? 2U : 1U;

        if (config->stop_mode == CF_STOP_AFTER_DURATION &&
            elapsed >= config->duration_ms) {
            break;
        }

        if (config->position_mode == CF_POSITION_FIXED) {
            CfAction move = {0};
            move.type = CF_ACTION_MOVE;
            move.x = config->fixed_x;
            move.y = config->fixed_y;
            result = emit_action(ops, move, &local_stats);
            if (result != CF_OK) {
                break;
            }
        } else {
            result = ops->cursor_position(ops->context, &click.x, &click.y);
            if (result != CF_OK) {
                break;
            }
        }

        click.type = CF_ACTION_CLICK;
        click.button = config->button;
        for (click_index = 0; click_index < clicks_per_cycle; ++click_index) {
            result = emit_action(ops, click, &local_stats);
            if (result != CF_OK) {
                break;
            }
        }
        if (result != CF_OK) {
            break;
        }
        ++local_stats.logical_clicks;

        if (config->stop_mode == CF_STOP_AFTER_COUNT &&
            local_stats.logical_clicks >= config->click_count) {
            break;
        }

        elapsed = ops->monotonic_ms(ops->context) - start;
        {
            uint32_t delay = config->interval_ms;
            CfWaitResult wait_result;

            if (config->stop_mode == CF_STOP_AFTER_DURATION) {
                uint64_t remaining = config->duration_ms - elapsed;
                if (remaining < delay) {
                    delay = (uint32_t)remaining;
                }
            }
            wait_result = ops->wait_ms(ops->context, delay);
            if (wait_result != CF_WAIT_ELAPSED) {
                result = wait_result == CF_WAIT_CANCELLED
                             ? CF_ERR_CANCELLED
                             : CF_ERR_PLATFORM;
                break;
            }
        }
    }

    local_stats.elapsed_ms = ops->monotonic_ms(ops->context) - start;
    if (stats != NULL) {
        *stats = local_stats;
    }
    return result;
}
