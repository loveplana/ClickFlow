#include "nuklear.h"

#include "ui/pages.h"
#include "ui/i18n.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <stdio.h>

static void hotkey_text(CfHotkey hotkey, CfLanguage language,
                        char *buffer, size_t size)
{
    if (cf_hotkey_format_utf8(hotkey, buffer, size) != CF_OK) {
        snprintf(buffer, size, "%s", cf_tr(language, "未设置", "Not set"));
    }
}

void cf_ui_draw_settings_page(struct nk_context *context, CfAppModel *model)
{
    char primary[64];
    char emergency[64];
    char label[96];
    double interval = model->config.clicker.interval_ms;
    CfLanguage language = model->config.language;

    hotkey_text(model->config.primary_hotkey, language, primary, sizeof(primary));
    hotkey_text(model->config.emergency_hotkey, language, emergency, sizeof(emergency));

    nk_layout_row_dynamic(context, 36.0f, 2);
    nk_label(context, cf_tr(language, "语言", "Language"), NK_TEXT_LEFT);
    if (cf_ui_primary_button(context,
                             cf_tr(language, "保存设置", "Save settings"),
                             false, model->config.theme)) {
        if (cf_app_save_settings(model) == CF_OK) {
            cf_app_show_toast(model,
                              cf_tr(language, "设置已保存", "Settings saved"));
        } else {
            cf_app_show_toast(model, cf_tr(
                language, "设置无效或热键已被其他程序占用",
                "Invalid settings or a hotkey is unavailable"));
        }
    }
    nk_layout_row_dynamic(context, 32.0f, 2);
    if (nk_option_label(context, "简体中文",
                        model->config.language == CF_LANGUAGE_ZH_CN) &&
        model->config.language != CF_LANGUAGE_ZH_CN) {
        cf_app_set_language(model, CF_LANGUAGE_ZH_CN);
    }
    if (nk_option_label(context, "English",
                        model->config.language == CF_LANGUAGE_EN_US) &&
        model->config.language != CF_LANGUAGE_EN_US) {
        cf_app_set_language(model, CF_LANGUAGE_EN_US);
    }

    nk_layout_row_dynamic(context, 24.0f, 1);
    nk_label(context, cf_tr(language, "外观", "Appearance"), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 32.0f, 2);
    if (nk_option_label(context, cf_tr(language, "深色", "Dark"), model->config.theme == CF_THEME_DARK)) {
        if (model->config.theme != CF_THEME_DARK) {
            model->config.theme = CF_THEME_DARK;
            model->config_dirty = true;
            cf_ui_apply_theme(context, model->config.theme);
        }
    }
    if (nk_option_label(context, cf_tr(language, "浅色", "Light"), model->config.theme == CF_THEME_LIGHT)) {
        if (model->config.theme != CF_THEME_LIGHT) {
            model->config.theme = CF_THEME_LIGHT;
            model->config_dirty = true;
            cf_ui_apply_theme(context, model->config.theme);
        }
    }

    nk_layout_row_dynamic(context, 24.0f, 1);
    nk_label(context, cf_tr(language, "全局热键", "Global hotkeys"), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 34.0f, 2);
    snprintf(label, sizeof(label), cf_tr(language, "启动 / 停止：%s", "Start / stop: %s"), primary);
    nk_label(context, label, NK_TEXT_LEFT);
    if (nk_button_label(context, model->capture == CF_CAPTURE_PRIMARY
                                     ? cf_tr(language, "请按组合键…", "Press shortcut…")
                                     : cf_tr(language, "重新设置", "Change"))) {
        model->capture = CF_CAPTURE_PRIMARY;
        cf_app_show_toast(model, cf_tr(language, "请按下启动 / 停止热键", "Press the start / stop shortcut"));
    }
    nk_layout_row_dynamic(context, 34.0f, 2);
    snprintf(label, sizeof(label), cf_tr(language, "紧急停止：%s", "Emergency stop: %s"), emergency);
    nk_label(context, label, NK_TEXT_LEFT);
    if (nk_button_label(context, model->capture == CF_CAPTURE_EMERGENCY
                                     ? cf_tr(language, "请按组合键…", "Press shortcut…")
                                     : cf_tr(language, "重新设置", "Change"))) {
        model->capture = CF_CAPTURE_EMERGENCY;
        cf_app_show_toast(model, cf_tr(language, "请按下紧急停止热键", "Press the emergency stop shortcut"));
    }
    nk_layout_row_dynamic(context, 20.0f, 1);
    nk_label(context, cf_tr(language,
             "热键在其他应用获得焦点时同样有效；重复键会被拒绝。",
             "Hotkeys work globally. Duplicate shortcuts are rejected."),
             NK_TEXT_LEFT);

    nk_layout_row_dynamic(context, 24.0f, 1);
    nk_label(context, cf_tr(language, "默认连点参数", "Clicker defaults"), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 32.0f, 1);
    if (nk_property_double(context, cf_tr(language, "默认间隔（毫秒）", "Default interval (ms)"), 1.0, &interval,
                           CF_CLICK_INTERVAL_MAX_MS, 1.0, 0.25f)) {
        model->config.clicker.interval_ms = (uint32_t)interval;
        model->config_dirty = true;
    }

}
