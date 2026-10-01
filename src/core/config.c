#include "core/config.h"

enum {
    CF_DEFAULT_PRIMARY_VK = 0x77,
    CF_DEFAULT_EMERGENCY_VK = 0x1B
};

void cf_config_defaults(CfConfig *config)
{
    if (config == NULL) {
        return;
    }

    config->theme = CF_THEME_DARK;
    config->language = CF_LANGUAGE_ZH_CN;
    config->clicker.button = CF_MOUSE_LEFT;
    config->clicker.click_kind = CF_CLICK_SINGLE;
    config->clicker.stop_mode = CF_STOP_MANUAL;
    config->clicker.position_mode = CF_POSITION_CURSOR;
    config->clicker.interval_ms = 100;
    config->clicker.click_count = 100;
    config->clicker.duration_ms = 10000;
    config->clicker.fixed_x = 0;
    config->clicker.fixed_y = 0;
    config->primary_hotkey = (CfHotkey){0, CF_DEFAULT_PRIMARY_VK};
    config->emergency_hotkey = (CfHotkey){0, CF_DEFAULT_EMERGENCY_VK};
}

CfResult cf_config_validate(const CfConfig *config,
                            const CfVirtualScreen *screen)
{
    CfResult result;

    if (config == NULL || (config->theme != CF_THEME_DARK &&
                           config->theme != CF_THEME_LIGHT) ||
        (config->language != CF_LANGUAGE_ZH_CN &&
         config->language != CF_LANGUAGE_EN_US)) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    result = cf_clicker_validate(&config->clicker, screen);
    if (result != CF_OK) {
        return result;
    }
    return cf_hotkey_validate_pair(config->primary_hotkey,
                                   config->emergency_hotkey);
}
