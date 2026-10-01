#ifndef CLICKFLOW_UI_APP_H
#define CLICKFLOW_UI_APP_H

#include "core/config.h"
#include "runtime/controller.h"

#include <stdbool.h>
#include <stddef.h>
#include <windows.h>

typedef enum CfPage {
    CF_PAGE_CLICKER,
    CF_PAGE_RECORDING,
    CF_PAGE_MACRO,
    CF_PAGE_SETTINGS
} CfPage;

typedef enum CfActivity {
    CF_ACTIVITY_NONE,
    CF_ACTIVITY_CLICKER,
    CF_ACTIVITY_RECORDING,
    CF_ACTIVITY_RECORDING_PLAYBACK,
    CF_ACTIVITY_MACRO
} CfActivity;

typedef enum CfHotkeyCapture {
    CF_CAPTURE_NONE,
    CF_CAPTURE_PRIMARY,
    CF_CAPTURE_EMERGENCY,
    CF_CAPTURE_MACRO
} CfHotkeyCapture;

typedef struct CfAppModel {
    CfPage page;
    CfConfig config;
    CfController *controller;
    HWND window;
    CfActivity activity;
    CfHotkeyCapture capture;
    CfMacro recording;
    CfMacro macro;
    CfRunOptions recording_options;
    CfRunOptions macro_options;
    int selected_action;
    char recording_name[CF_MACRO_NAME_MAX_BYTES + 1U];
    char macro_name[CF_MACRO_NAME_MAX_BYTES + 1U];
    bool recording_ready;
    bool recording_dirty;
    bool macro_ready;
    bool config_dirty;
    char toast[256];
    ULONGLONG toast_until_ms;
    BOOL animations_enabled;
} CfAppModel;

void cf_app_show_toast(CfAppModel *model, const char *message);
void cf_app_set_language(CfAppModel *model, CfLanguage language);
CfResult cf_app_save_settings(CfAppModel *model);
CfResult cf_app_refresh_hotkeys(CfAppModel *model);
CfVirtualScreen cf_app_virtual_screen(void);

int cf_app_run(HINSTANCE instance, int show_command);

#endif
