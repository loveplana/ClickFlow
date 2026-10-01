#include "nuklear.h"

#include "ui/pages.h"
#include "ui/i18n.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <stdint.h>
#include <stdio.h>

static bool task_active(const CfAppModel *model)
{
    CfTaskState state = cf_controller_state(model->controller);
    return state == CF_TASK_STARTING || state == CF_TASK_RUNNING ||
           state == CF_TASK_PAUSED || state == CF_TASK_STOPPING;
}

static void mark_changed(CfAppModel *model)
{
    model->config_dirty = true;
}

void cf_ui_draw_clicker_page(struct nk_context *context, CfAppModel *model)
{
    static const char *buttons_zh[] = {"左键", "中键", "右键"};
    static const char *buttons_en[] = {"Left", "Middle", "Right"};
    const char **buttons = model->config.language == CF_LANGUAGE_EN_US
                              ? buttons_en : buttons_zh;
    CfLanguage language = model->config.language;
    CfClickerConfig *config = &model->config.clicker;
    CfTheme theme = model->config.theme;
    bool active = task_active(model);
    int selected;
    double value;
    char primary[64] = "F8";
    char emergency[64] = "Esc";
    char action_label[128];

    cf_hotkey_format_utf8(model->config.primary_hotkey, primary,
                          sizeof(primary));
    cf_hotkey_format_utf8(model->config.emergency_hotkey, emergency,
                          sizeof(emergency));

    nk_layout_row_dynamic(context, 26.0f, 1);
    nk_label(context, cf_tr(language, "点击方式", "Click style"), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 34.0f, 2);
    selected = (int)config->button;
    selected = nk_combo(context, buttons, 3, selected, 28,
                        nk_vec2(180.0f, 130.0f));
    if (selected != (int)config->button) {
        config->button = (CfMouseButton)selected;
        mark_changed(model);
    }
    if (nk_option_label(context, cf_tr(language, "单击", "Single click"), config->click_kind == CF_CLICK_SINGLE)) {
        if (config->click_kind != CF_CLICK_SINGLE) mark_changed(model);
        config->click_kind = CF_CLICK_SINGLE;
    }
    nk_layout_row_dynamic(context, 34.0f, 2);
    nk_spacing(context, 1);
    if (nk_option_label(context, cf_tr(language, "双击", "Double click"), config->click_kind == CF_CLICK_DOUBLE)) {
        if (config->click_kind != CF_CLICK_DOUBLE) mark_changed(model);
        config->click_kind = CF_CLICK_DOUBLE;
    }

    nk_layout_row_dynamic(context, 26.0f, 1);
    nk_label(context, cf_tr(language, "节奏与结束条件", "Timing and stop condition"), NK_TEXT_LEFT);
    value = config->interval_ms;
    nk_layout_row_dynamic(context, 34.0f, 1);
    if (nk_property_double(context, cf_tr(language, "间隔（毫秒）", "Interval (ms)"), 1.0, &value,
                           CF_CLICK_INTERVAL_MAX_MS, 1.0, 0.25f)) {
        config->interval_ms = (uint32_t)value;
        mark_changed(model);
    }
    nk_layout_row_dynamic(context, 32.0f, 3);
    if (nk_option_label(context, cf_tr(language, "手动停止", "Manual"), config->stop_mode == CF_STOP_MANUAL)) {
        config->stop_mode = CF_STOP_MANUAL;
        mark_changed(model);
    }
    if (nk_option_label(context, cf_tr(language, "按次数", "Click count"), config->stop_mode == CF_STOP_AFTER_COUNT)) {
        config->stop_mode = CF_STOP_AFTER_COUNT;
        mark_changed(model);
    }
    if (nk_option_label(context, cf_tr(language, "按时长", "Duration"), config->stop_mode == CF_STOP_AFTER_DURATION)) {
        config->stop_mode = CF_STOP_AFTER_DURATION;
        mark_changed(model);
    }
    if (config->stop_mode == CF_STOP_AFTER_COUNT) {
        value = (double)config->click_count;
        nk_layout_row_dynamic(context, 34.0f, 1);
        if (nk_property_double(context, cf_tr(language, "点击次数", "Clicks"), 1.0, &value,
                               (double)CF_CLICK_COUNT_MAX, 1.0, 0.25f)) {
            config->click_count = (uint64_t)value;
            mark_changed(model);
        }
    } else if (config->stop_mode == CF_STOP_AFTER_DURATION) {
        value = config->duration_ms;
        nk_layout_row_dynamic(context, 34.0f, 1);
        if (nk_property_double(context, cf_tr(language, "持续（毫秒）", "Duration (ms)"), 1.0, &value,
                               CF_CLICK_DURATION_MAX_MS, 100.0, 0.25f)) {
            config->duration_ms = (uint32_t)value;
            mark_changed(model);
        }
    }

    nk_layout_row_dynamic(context, 26.0f, 1);
    nk_label(context, cf_tr(language, "点击位置", "Click position"), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 32.0f, 2);
    if (nk_option_label(context, cf_tr(language, "跟随光标", "Follow cursor"), config->position_mode == CF_POSITION_CURSOR)) {
        config->position_mode = CF_POSITION_CURSOR;
        mark_changed(model);
    }
    if (nk_option_label(context, cf_tr(language, "固定坐标", "Fixed position"), config->position_mode == CF_POSITION_FIXED)) {
        config->position_mode = CF_POSITION_FIXED;
        mark_changed(model);
    }
    if (config->position_mode == CF_POSITION_FIXED) {
        nk_layout_row_dynamic(context, 34.0f, 3);
        if (nk_property_int(context, "X", INT32_MIN, &config->fixed_x,
                            INT32_MAX, 1, 0.25f)) mark_changed(model);
        if (nk_property_int(context, "Y", INT32_MIN, &config->fixed_y,
                            INT32_MAX, 1, 0.25f)) mark_changed(model);
        if (nk_button_label(context, cf_tr(language, "取当前坐标", "Use cursor"))) {
            POINT cursor;
            if (GetCursorPos(&cursor)) {
                config->fixed_x = cursor.x;
                config->fixed_y = cursor.y;
                mark_changed(model);
            }
        }
    }

    nk_layout_row_dynamic(context, 12.0f, 1);
    nk_spacing(context, 1);
    nk_layout_row_dynamic(context, 44.0f, 1);
    if (active && model->activity == CF_ACTIVITY_CLICKER) {
        snprintf(action_label, sizeof(action_label), cf_tr(language, "停止连点（%s）", "Stop clicking (%s)"), emergency);
        if (cf_ui_primary_button(context, action_label, true, theme)) {
            cf_controller_stop(model->controller);
            cf_app_show_toast(model, cf_tr(language, "正在停止连点", "Stopping clicker"));
        }
    } else if (!active) {
        snprintf(action_label, sizeof(action_label), cf_tr(language, "开始连点（%s）", "Start clicking (%s)"), primary);
        if (cf_ui_primary_button(context, action_label, false, theme)) {
            CfResult result = cf_controller_start_clicker(model->controller, config);
            if (result == CF_OK) {
                model->activity = CF_ACTIVITY_CLICKER;
                cf_app_show_toast(model, cf_tr(language, "连点已开始，按 Esc 随时停止", "Clicker started. Press Esc to stop"));
            } else {
                cf_app_show_toast(model, cf_tr(language, "参数无效或任务正忙", "Invalid settings or another task is busy"));
            }
        }
    } else {
        nk_button_label(context, cf_tr(language, "另一项任务正在运行", "Another task is running"));
    }
}
