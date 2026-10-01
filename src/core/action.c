#include "core/action.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
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

static bool same_screen(const CfVirtualScreen *left,
                        const CfVirtualScreen *right)
{
    return left->left == right->left && left->top == right->top &&
           left->width == right->width && left->height == right->height;
}

static bool point_on_screen(int32_t x, int32_t y,
                            const CfVirtualScreen *screen)
{
    int64_t right = (int64_t)screen->left + screen->width;
    int64_t bottom = (int64_t)screen->top + screen->height;

    return x >= screen->left && (int64_t)x < right && y >= screen->top &&
           (int64_t)y < bottom;
}

void cf_action_list_init(CfActionList *list)
{
    if (list != NULL) {
        list->items = NULL;
        list->count = 0;
        list->capacity = 0;
    }
}

void cf_action_list_free(CfActionList *list)
{
    if (list != NULL) {
        free(list->items);
        cf_action_list_init(list);
    }
}

CfResult cf_action_list_push(CfActionList *list, CfAction action)
{
    CfAction *grown;
    size_t capacity;

    if (list == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (list->count >= CF_ACTION_LIMIT) {
        return CF_ERR_LIMIT;
    }
    if (list->count < list->capacity) {
        list->items[list->count++] = action;
        return CF_OK;
    }

    capacity = list->capacity == 0 ? 8U : list->capacity * 2U;
    if (capacity > CF_ACTION_LIMIT) {
        capacity = CF_ACTION_LIMIT;
    }
    if (capacity <= list->count || capacity > SIZE_MAX / sizeof(*grown)) {
        return CF_ERR_LIMIT;
    }

    grown = realloc(list->items, capacity * sizeof(*grown));
    if (grown == NULL) {
        return CF_ERR_OUT_OF_MEMORY;
    }
    list->items = grown;
    list->capacity = capacity;
    list->items[list->count++] = action;
    return CF_OK;
}

CfResult cf_action_list_copy(CfActionList *destination,
                             const CfActionList *source)
{
    CfActionList copy;

    if (destination == NULL || source == NULL ||
        source->count > CF_ACTION_LIMIT ||
        (source->count != 0 && source->items == NULL)) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (destination == source) {
        return CF_OK;
    }

    cf_action_list_init(&copy);
    if (source->count != 0) {
        copy.items = malloc(source->count * sizeof(*copy.items));
        if (copy.items == NULL) {
            return CF_ERR_OUT_OF_MEMORY;
        }
        memcpy(copy.items, source->items,
               source->count * sizeof(*copy.items));
        copy.count = source->count;
        copy.capacity = source->count;
    }

    cf_action_list_free(destination);
    *destination = copy;
    return CF_OK;
}

void cf_macro_init(CfMacro *macro)
{
    if (macro != NULL) {
        macro->name = NULL;
        cf_action_list_init(&macro->actions);
        macro->repeat_count = 1;
        macro->playback_speed = 1.0;
        macro->recorded_screen = (CfVirtualScreen){0, 0, 0, 0};
        macro->has_hotkey = false;
        macro->hotkey = (CfHotkey){0, 0};
    }
}

void cf_macro_free(CfMacro *macro)
{
    if (macro != NULL) {
        free(macro->name);
        cf_action_list_free(&macro->actions);
        cf_macro_init(macro);
    }
}

CfResult cf_macro_set_name(CfMacro *macro, const char *name)
{
    char *copy;
    size_t length;

    if (macro == NULL || name == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    length = strlen(name);
    if (length == 0 || length > CF_MACRO_NAME_MAX_BYTES) {
        return CF_ERR_INVALID_ARGUMENT;
    }

    copy = malloc(length + 1U);
    if (copy == NULL) {
        return CF_ERR_OUT_OF_MEMORY;
    }
    memcpy(copy, name, length + 1U);
    free(macro->name);
    macro->name = copy;
    return CF_OK;
}

CfResult cf_macro_copy(CfMacro *destination, const CfMacro *source)
{
    CfMacro copy;
    CfResult result;

    if (destination == NULL || source == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (destination == source) {
        return CF_OK;
    }

    cf_macro_init(&copy);
    result = cf_macro_set_name(&copy, source->name);
    if (result != CF_OK) {
        return result;
    }
    result = cf_action_list_copy(&copy.actions, &source->actions);
    if (result != CF_OK) {
        cf_macro_free(&copy);
        return result;
    }
    copy.repeat_count = source->repeat_count;
    copy.playback_speed = source->playback_speed;
    copy.recorded_screen = source->recorded_screen;
    copy.has_hotkey = source->has_hotkey;
    copy.hotkey = source->hotkey;

    cf_macro_free(destination);
    *destination = copy;
    return CF_OK;
}

CfResult cf_macro_validate(const CfMacro *macro,
                           const CfVirtualScreen *current_screen)
{
    bool pressed[3] = {false, false, false};
    size_t index;

    if (macro == NULL || !valid_screen(current_screen) ||
        macro->name == NULL || macro->name[0] == '\0' ||
        strlen(macro->name) > CF_MACRO_NAME_MAX_BYTES ||
        macro->actions.count == 0 ||
        macro->actions.count > CF_ACTION_LIMIT ||
        macro->actions.items == NULL || macro->repeat_count == 0 ||
        macro->repeat_count > CF_MACRO_REPEAT_MAX ||
        !isfinite(macro->playback_speed) || macro->playback_speed < 0.1 ||
        macro->playback_speed > 10.0 ||
        !valid_screen(&macro->recorded_screen) ||
        (macro->has_hotkey && cf_hotkey_validate(&macro->hotkey) != CF_OK)) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (!same_screen(&macro->recorded_screen, current_screen)) {
        return CF_ERR_CONFLICT;
    }

    for (index = 0; index < macro->actions.count; ++index) {
        const CfAction *action = &macro->actions.items[index];

        if (action->type < CF_ACTION_MOVE || action->type > CF_ACTION_WAIT ||
            action->delay_ms > CF_ACTION_DELAY_MAX_MS) {
            return CF_ERR_INVALID_ARGUMENT;
        }

        switch (action->type) {
        case CF_ACTION_MOVE:
            if (!point_on_screen(action->x, action->y, current_screen)) {
                return CF_ERR_INVALID_ARGUMENT;
            }
            break;
        case CF_ACTION_BUTTON_DOWN:
            if (!valid_button(action->button) || pressed[action->button]) {
                return CF_ERR_INVALID_ARGUMENT;
            }
            pressed[action->button] = true;
            break;
        case CF_ACTION_BUTTON_UP:
            if (!valid_button(action->button) || !pressed[action->button]) {
                return CF_ERR_INVALID_ARGUMENT;
            }
            pressed[action->button] = false;
            break;
        case CF_ACTION_CLICK:
            if (!valid_button(action->button)) {
                return CF_ERR_INVALID_ARGUMENT;
            }
            break;
        case CF_ACTION_WHEEL:
        case CF_ACTION_WAIT:
            break;
        }
    }

    if (pressed[CF_MOUSE_LEFT] || pressed[CF_MOUSE_MIDDLE] ||
        pressed[CF_MOUSE_RIGHT]) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    return CF_OK;
}
