#include "nuklear.h"

#include "storage/json_store.h"
#include "ui/i18n.h"
#include "ui/pages.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

static bool active_state(CfTaskState state)
{
    return state == CF_TASK_STARTING || state == CF_TASK_RUNNING ||
           state == CF_TASK_PAUSED || state == CF_TASK_STOPPING;
}

static void set_name_buffer(CfAppModel *model)
{
    snprintf(model->macro_name, sizeof(model->macro_name), "%s",
             model->macro.name != NULL ? model->macro.name : "");
}

static void reset_macro(CfAppModel *model)
{
    cf_macro_free(&model->macro);
    cf_macro_init(&model->macro);
    cf_macro_set_name(&model->macro,
                      cf_tr(model->config.language, "新建宏", "New macro"));
    model->macro.recorded_screen = cf_app_virtual_screen();
    model->macro_options = (CfRunOptions){false, 1, 1.0};
    model->selected_action = -1;
    model->macro_ready = true;
    set_name_buffer(model);
}

static const char *action_name(CfActionType type, CfLanguage language)
{
    static const char *zh[] = {"移动", "按下", "抬起", "点击", "滚轮", "等待"};
    static const char *en[] = {"Move", "Press", "Release", "Click", "Wheel", "Wait"};
    const char **names = language == CF_LANGUAGE_EN_US ? en : zh;
    return type >= CF_ACTION_MOVE && type <= CF_ACTION_WAIT
               ? names[type] : cf_tr(language, "未知", "Unknown");
}

static void add_action(CfAppModel *model, CfAction action)
{
    if (!model->macro_ready) reset_macro(model);
    if (cf_action_list_push(&model->macro.actions, action) == CF_OK) {
        model->selected_action = (int)model->macro.actions.count - 1;
        cf_app_show_toast(model, cf_tr(model->config.language, "动作已添加", "Action added"));
    } else {
        cf_app_show_toast(model, cf_tr(model->config.language, "动作数量已达上限", "Action limit reached"));
    }
}

static void remove_action(CfAppModel *model)
{
    CfActionList *list = &model->macro.actions;
    size_t selected;

    if (model->selected_action < 0 ||
        (size_t)model->selected_action >= list->count) return;
    selected = (size_t)model->selected_action;
    memmove(&list->items[selected], &list->items[selected + 1U],
            (list->count - selected - 1U) * sizeof(*list->items));
    --list->count;
    if (list->count == 0) model->selected_action = -1;
    else if ((size_t)model->selected_action >= list->count) {
        model->selected_action = (int)list->count - 1;
    }
}

static void move_action(CfAppModel *model, int direction)
{
    CfActionList *list = &model->macro.actions;
    int target = model->selected_action + direction;
    CfAction temporary;

    if (model->selected_action < 0 || target < 0 ||
        (size_t)target >= list->count) return;
    temporary = list->items[model->selected_action];
    list->items[model->selected_action] = list->items[target];
    list->items[target] = temporary;
    model->selected_action = target;
}

static void duplicate_action(CfAppModel *model)
{
    if (model->selected_action < 0 ||
        (size_t)model->selected_action >= model->macro.actions.count) return;
    add_action(model, model->macro.actions.items[model->selected_action]);
}

static bool prepare_macro(CfAppModel *model)
{
    if (!model->macro_ready ||
        cf_macro_set_name(&model->macro, model->macro_name) != CF_OK) {
        cf_app_show_toast(model, cf_tr(model->config.language, "请输入有效的宏名称", "Enter a valid macro name"));
        return false;
    }
    model->macro.repeat_count = model->macro_options.repeat_count;
    model->macro.playback_speed = model->macro_options.playback_speed;
    if (cf_macro_validate(&model->macro, &model->macro.recorded_screen) != CF_OK) {
        cf_app_show_toast(model, cf_tr(model->config.language, "动作序列无效，请检查坐标和按下/抬起顺序", "Invalid action sequence. Check coordinates and press/release order"));
        return false;
    }
    return true;
}

static void save_macro(CfAppModel *model)
{
    wchar_t path[MAX_PATH];
    char error[160];

    if (!prepare_macro(model)) return;
    wcscpy(path, L"macro.cfm.json");
    if (!cf_ui_save_json_path(model->window, model->config.language,
                              path, CF_ARRAY_COUNT(path))) return;
    if (cf_json_save_macro_atomic(path, "macro", &model->macro,
                                  error, sizeof(error)) == CF_OK) {
        cf_app_show_toast(model, cf_tr(model->config.language, "宏已保存", "Macro saved"));
    } else {
        cf_app_show_toast(model, model->config.language == CF_LANGUAGE_EN_US
                                     ? "Could not save macro"
                                     : error[0] != '\0' ? error : "保存失败");
    }
}

static void load_macro(CfAppModel *model)
{
    wchar_t path[MAX_PATH];
    char error[160];

    if (!cf_ui_open_json_path(model->window, model->config.language,
                              path, CF_ARRAY_COUNT(path))) return;
    if (cf_json_load_macro(path, "macro", &model->macro,
                           error, sizeof(error)) == CF_OK) {
        model->macro_ready = true;
        model->macro_options.run_forever = false;
        model->macro_options.repeat_count = model->macro.repeat_count;
        model->macro_options.playback_speed = model->macro.playback_speed;
        model->selected_action = model->macro.actions.count != 0 ? 0 : -1;
        set_name_buffer(model);
        if (cf_app_refresh_hotkeys(model) == CF_OK) {
            cf_app_show_toast(model, cf_tr(model->config.language, "宏已载入", "Macro loaded"));
        } else {
            cf_app_show_toast(model, cf_tr(model->config.language, "宏已载入，但专属热键不可用", "Macro loaded, but its hotkey is unavailable"));
        }
    } else {
        cf_app_show_toast(model, model->config.language == CF_LANGUAGE_EN_US
                                     ? "Could not load macro"
                                     : error[0] != '\0' ? error : "载入失败");
    }
}

static void delete_macro_file(CfAppModel *model)
{
    wchar_t path[MAX_PATH];

    if (!cf_ui_open_json_path(model->window, model->config.language,
                              path, CF_ARRAY_COUNT(path))) return;
    if (MessageBoxW(model->window,
                    cf_trw(model->config.language, L"确定删除这个宏文件？",
                           L"Delete this macro file?"), L"ClickFlow",
                    MB_ICONWARNING | MB_OKCANCEL) != IDOK) return;
    if (DeleteFileW(path)) {
        reset_macro(model);
        cf_app_show_toast(model, cf_tr(model->config.language, "宏文件已删除", "Macro file deleted"));
    } else {
        cf_app_show_toast(model, cf_tr(model->config.language, "删除失败", "Could not delete file"));
    }
}

static void draw_action_editor(struct nk_context *context, CfAppModel *model)
{
    static const char *types_zh[] = {"移动", "按下", "抬起", "点击", "滚轮", "等待"};
    static const char *types_en[] = {"Move", "Press", "Release", "Click", "Wheel", "Wait"};
    static const char *buttons_zh[] = {"左键", "中键", "右键"};
    static const char *buttons_en[] = {"Left", "Middle", "Right"};
    CfLanguage language = model->config.language;
    const char **types = language == CF_LANGUAGE_EN_US ? types_en : types_zh;
    const char **buttons = language == CF_LANGUAGE_EN_US ? buttons_en : buttons_zh;
    CfAction *action;
    int selected;
    double delay;

    if (model->selected_action < 0 ||
        (size_t)model->selected_action >= model->macro.actions.count) {
        nk_layout_row_dynamic(context, 24.0f, 1);
        nk_label(context, cf_tr(language, "选择一个动作后可编辑参数", "Select an action to edit its settings"), NK_TEXT_LEFT);
        return;
    }
    action = &model->macro.actions.items[model->selected_action];
    nk_layout_row_dynamic(context, 32.0f, 2);
    selected = nk_combo(context, types, 6, (int)action->type, 26,
                        nk_vec2(160.0f, 190.0f));
    action->type = (CfActionType)selected;
    delay = action->delay_ms;
    if (nk_property_double(context, cf_tr(language, "延迟 ms", "Delay ms"), 0.0, &delay,
                           CF_ACTION_DELAY_MAX_MS, 1.0, 0.25f)) {
        action->delay_ms = (uint32_t)delay;
    }
    if (action->type == CF_ACTION_MOVE) {
        nk_layout_row_dynamic(context, 32.0f, 2);
        nk_property_int(context, "X", INT32_MIN, &action->x, INT32_MAX, 1, 0.25f);
        nk_property_int(context, "Y", INT32_MIN, &action->y, INT32_MAX, 1, 0.25f);
    } else if (action->type == CF_ACTION_BUTTON_DOWN ||
               action->type == CF_ACTION_BUTTON_UP ||
               action->type == CF_ACTION_CLICK) {
        nk_layout_row_dynamic(context, 32.0f, 1);
        action->button = (CfMouseButton)nk_combo(context, buttons, 3,
                                                 (int)action->button, 26,
                                                 nk_vec2(180.0f, 130.0f));
    } else if (action->type == CF_ACTION_WHEEL) {
        nk_layout_row_dynamic(context, 32.0f, 1);
        nk_property_int(context, cf_tr(language, "滚轮增量", "Wheel delta"), INT32_MIN, &action->wheel_delta,
                        INT32_MAX, 120, 0.25f);
    }
}

void cf_ui_draw_macro_page(struct nk_context *context, CfAppModel *model)
{
    CfTaskState state = cf_controller_state(model->controller);
    bool active = active_state(state);
    char label[96];
    char hotkey[64];
    size_t index;
    double value;
    CfLanguage language = model->config.language;

    snprintf(hotkey, sizeof(hotkey), "%s", cf_tr(language, "未设置", "Not set"));

    nk_layout_row_dynamic(context, 34.0f, 4);
    if (nk_button_label(context, cf_tr(language, "新建", "New"))) reset_macro(model);
    if (nk_button_label(context, cf_tr(language, "载入", "Load"))) load_macro(model);
    if (nk_button_label(context, cf_tr(language, "保存", "Save"))) save_macro(model);
    if (nk_button_label(context, cf_tr(language, "删除文件", "Delete file"))) delete_macro_file(model);

    nk_layout_row_dynamic(context, 32.0f, 1);
    nk_edit_string_zero_terminated(context, NK_EDIT_FIELD, model->macro_name,
                                   (int)sizeof(model->macro_name), nk_filter_default);

    nk_layout_row_dynamic(context, 32.0f, 4);
    if (nk_button_label(context, cf_tr(language, "+ 移动", "+ Move"))) {
        POINT point = {0, 0};
        GetCursorPos(&point);
        add_action(model, (CfAction){CF_ACTION_MOVE, 0, point.x, point.y, 0,
                                     CF_MOUSE_LEFT});
    }
    if (nk_button_label(context, cf_tr(language, "+ 点击", "+ Click"))) {
        add_action(model, (CfAction){CF_ACTION_CLICK, 100, 0, 0, 0,
                                     CF_MOUSE_LEFT});
    }
    if (nk_button_label(context, cf_tr(language, "+ 等待", "+ Wait"))) {
        add_action(model, (CfAction){CF_ACTION_WAIT, 500, 0, 0, 0,
                                     CF_MOUSE_LEFT});
    }
    if (nk_button_label(context, cf_tr(language, "+ 滚轮", "+ Wheel"))) {
        add_action(model, (CfAction){CF_ACTION_WHEEL, 100, 0, 0, 120,
                                     CF_MOUSE_LEFT});
    }

    nk_layout_row_dynamic(context, 126.0f, 1);
    if (nk_group_begin(context, "MacroActions", NK_WINDOW_BORDER)) {
        for (index = 0; index < model->macro.actions.count; ++index) {
            const CfAction *action = &model->macro.actions.items[index];
            snprintf(label, sizeof(label), cf_tr(language,
                     "%s%03llu  %s  ·  延迟 %u ms",
                     "%s%03llu  %s  ·  delay %u ms"),
                     (int)index == model->selected_action ? "● " : "○ ",
                     (unsigned long long)(index + 1U), action_name(action->type, language),
                     (unsigned)action->delay_ms);
            nk_layout_row_dynamic(context, 28.0f, 1);
            if (nk_button_label(context, label)) model->selected_action = (int)index;
        }
        if (model->macro.actions.count == 0) {
            nk_layout_row_dynamic(context, 26.0f, 1);
            nk_label(context, cf_tr(language, "添加动作以创建自动化流程", "Add actions to build an automation"), NK_TEXT_CENTERED);
        }
        nk_group_end(context);
    }

    nk_layout_row_dynamic(context, 30.0f, 4);
    if (nk_button_label(context, cf_tr(language, "上移", "Move up"))) move_action(model, -1);
    if (nk_button_label(context, cf_tr(language, "下移", "Move down"))) move_action(model, 1);
    if (nk_button_label(context, cf_tr(language, "复制", "Duplicate"))) duplicate_action(model);
    if (nk_button_label(context, cf_tr(language, "删除动作", "Delete"))) remove_action(model);
    draw_action_editor(context, model);

    nk_layout_row_dynamic(context, 32.0f, 2);
    value = model->macro_options.playback_speed;
    if (nk_property_double(context, cf_tr(language, "速度", "Speed"), 0.1, &value, 10.0, 0.1, 0.05f)) {
        model->macro_options.playback_speed = value;
    }
    value = model->macro_options.repeat_count;
    if (nk_property_double(context, cf_tr(language, "次数", "Repeats"), 1.0, &value,
                           CF_MACRO_REPEAT_MAX, 1.0, 0.25f)) {
        model->macro_options.repeat_count = (uint32_t)value;
    }

    if (model->macro.has_hotkey) {
        cf_hotkey_format_utf8(model->macro.hotkey, hotkey, sizeof(hotkey));
    }
    snprintf(label, sizeof(label), cf_tr(language, "宏热键：%s", "Macro hotkey: %s"), hotkey);
    nk_layout_row_dynamic(context, 32.0f, 3);
    nk_label(context, label, NK_TEXT_LEFT);
    if (nk_button_label(context, model->capture == CF_CAPTURE_MACRO
                                      ? cf_tr(language, "请按组合键…", "Press shortcut…")
                                      : cf_tr(language, "设置热键", "Set hotkey"))) {
        model->capture = CF_CAPTURE_MACRO;
        cf_app_show_toast(model, cf_tr(language, "请按下新的宏热键", "Press the new macro hotkey"));
    }
    if (nk_button_label(context, cf_tr(language, "清除热键", "Clear hotkey"))) {
        model->macro.has_hotkey = false;
        model->capture = CF_CAPTURE_NONE;
        cf_app_refresh_hotkeys(model);
    }

    nk_layout_row_dynamic(context, 42.0f, 3);
    if (!active) {
        if (cf_ui_primary_button(context, cf_tr(language, "运行宏", "Run macro"), false, model->config.theme)) {
            CfVirtualScreen screen = cf_app_virtual_screen();
            if (prepare_macro(model) &&
                cf_macro_validate(&model->macro, &screen) == CF_OK &&
                cf_controller_start_macro(model->controller, &model->macro,
                                          &model->macro_options) == CF_OK) {
                model->activity = CF_ACTIVITY_MACRO;
                cf_app_show_toast(model, cf_tr(language, "宏正在运行，按 Esc 停止", "Macro running. Press Esc to stop"));
            } else {
                cf_app_show_toast(model, cf_tr(language, "宏无效或屏幕布局与录制时不同", "Macro is invalid or the display layout changed"));
            }
        }
    } else if (model->activity == CF_ACTIVITY_MACRO) {
        if (state == CF_TASK_PAUSED) {
            if (nk_button_label(context, cf_tr(language, "继续", "Resume"))) cf_controller_resume(model->controller);
        } else if (nk_button_label(context, cf_tr(language, "暂停", "Pause"))) {
            cf_controller_pause(model->controller);
        }
    } else {
        nk_button_label(context, cf_tr(language, "任务运行中", "Task running"));
    }
    if (active && model->activity == CF_ACTIVITY_MACRO) {
        if (cf_ui_primary_button(context, cf_tr(language, "停止", "Stop"), true, model->config.theme)) {
            cf_controller_stop(model->controller);
        }
    } else {
        nk_spacing(context, 1);
    }
    nk_spacing(context, 1);
}
