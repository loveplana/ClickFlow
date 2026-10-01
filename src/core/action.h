#ifndef CLICKFLOW_ACTION_H
#define CLICKFLOW_ACTION_H

#include "core/cf_result.h"
#include "core/hotkey.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CF_ACTION_LIMIT 1000000U
#define CF_ACTION_DELAY_MAX_MS 2592000000U
#define CF_MACRO_NAME_MAX_BYTES 255U
#define CF_MACRO_REPEAT_MAX 1000000U

typedef enum CfActionType {
    CF_ACTION_MOVE,
    CF_ACTION_BUTTON_DOWN,
    CF_ACTION_BUTTON_UP,
    CF_ACTION_CLICK,
    CF_ACTION_WHEEL,
    CF_ACTION_WAIT
} CfActionType;

typedef enum CfMouseButton {
    CF_MOUSE_LEFT,
    CF_MOUSE_MIDDLE,
    CF_MOUSE_RIGHT
} CfMouseButton;

typedef struct CfVirtualScreen {
    int32_t left;
    int32_t top;
    int32_t width;
    int32_t height;
} CfVirtualScreen;

typedef struct CfAction {
    CfActionType type;
    uint32_t delay_ms;
    int32_t x;
    int32_t y;
    int32_t wheel_delta;
    CfMouseButton button;
} CfAction;

typedef struct CfActionList {
    CfAction *items;
    size_t count;
    size_t capacity;
} CfActionList;

typedef struct CfMacro {
    char *name;
    CfActionList actions;
    uint32_t repeat_count;
    double playback_speed;
    CfVirtualScreen recorded_screen;
    bool has_hotkey;
    CfHotkey hotkey;
} CfMacro;

void cf_action_list_init(CfActionList *list);
void cf_action_list_free(CfActionList *list);
CfResult cf_action_list_push(CfActionList *list, CfAction action);
CfResult cf_action_list_copy(CfActionList *destination,
                             const CfActionList *source);

void cf_macro_init(CfMacro *macro);
void cf_macro_free(CfMacro *macro);
CfResult cf_macro_set_name(CfMacro *macro, const char *name);
CfResult cf_macro_copy(CfMacro *destination, const CfMacro *source);
CfResult cf_macro_validate(const CfMacro *macro,
                           const CfVirtualScreen *current_screen);

#endif
