#ifndef CLICKFLOW_CONFIG_H
#define CLICKFLOW_CONFIG_H

#include "core/clicker.h"
#include "core/hotkey.h"

typedef enum CfTheme {
    CF_THEME_DARK,
    CF_THEME_LIGHT
} CfTheme;

typedef enum CfLanguage {
    CF_LANGUAGE_ZH_CN,
    CF_LANGUAGE_EN_US
} CfLanguage;

typedef struct CfConfig {
    CfTheme theme;
    CfLanguage language;
    CfClickerConfig clicker;
    CfHotkey primary_hotkey;
    CfHotkey emergency_hotkey;
} CfConfig;

void cf_config_defaults(CfConfig *config);
CfResult cf_config_validate(const CfConfig *config,
                            const CfVirtualScreen *screen);

#endif
