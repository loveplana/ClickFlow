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

static void copy_macro_name(char *destination, const char *source)
{
    snprintf(destination, CF_MACRO_NAME_MAX_BYTES + 1U, "%s",
             source != NULL ? source : "");
}

static void save_recording(CfAppModel *model)
{
    wchar_t path[MAX_PATH];
    char error[160];

    if (!model->recording_ready) {
        cf_app_show_toast(model, cf_tr(model->config.language, "还没有可保存的录制", "There is no recording to save"));
        return;
    }
    wcscpy(path, L"recording.cfr.json");
    if (!cf_ui_save_json_path(model->window, model->config.language,
                              path, CF_ARRAY_COUNT(path))) return;
    if (cf_json_save_macro_atomic(path, "recording", &model->recording,
                                  error, sizeof(error)) == CF_OK) {
        model->recording_dirty = false;
        cf_app_show_toast(model, cf_tr(model->config.language, "录制已保存", "Recording saved"));
    } else {
        cf_app_show_toast(model, model->config.language == CF_LANGUAGE_EN_US
                                     ? "Could not save recording"
                                     : error[0] != '\0' ? error : "保存失败");
    }
}

static void load_recording(CfAppModel *model)
{
    wchar_t path[MAX_PATH];
    char error[160];

    if (model->recording_dirty &&
        MessageBoxW(model->window, cf_trw(model->config.language,
                    L"当前录制尚未保存，确定丢弃并载入其他文件？",
                    L"This recording has not been saved. Discard it and load another file?"),
                    L"ClickFlow", MB_ICONWARNING | MB_OKCANCEL) != IDOK) return;
    if (!cf_ui_open_json_path(model->window, model->config.language,
                              path, CF_ARRAY_COUNT(path))) return;
    if (cf_json_load_macro(path, "recording", &model->recording,
                           error, sizeof(error)) == CF_OK) {
        model->recording_ready = true;
        model->recording_dirty = false;
        model->recording_options.run_forever = false;
        model->recording_options.repeat_count = model->recording.repeat_count;
        model->recording_options.playback_speed = model->recording.playback_speed;
        copy_macro_name(model->recording_name, model->recording.name);
        cf_app_show_toast(model, cf_tr(model->config.language, "录制已载入", "Recording loaded"));
    } else {
        cf_app_show_toast(model, model->config.language == CF_LANGUAGE_EN_US
                                     ? "Could not load recording"
                                     : error[0] != '\0' ? error : "载入失败");
    }
}

static void delete_recording_file(CfAppModel *model)
{
    wchar_t path[MAX_PATH];

    if (!cf_ui_open_json_path(model->window, model->config.language,
                              path, CF_ARRAY_COUNT(path))) return;
    if (MessageBoxW(model->window, cf_trw(model->config.language,
                    L"确定删除这个录制文件？",
                    L"Delete this recording file?"),
                    L"ClickFlow", MB_ICONWARNING | MB_OKCANCEL) != IDOK) {
        return;
    }
    if (DeleteFileW(path)) {
        cf_macro_free(&model->recording);
        model->recording_ready = false;
        model->recording_dirty = false;
        model->recording_name[0] = '\0';
        cf_app_show_toast(model, cf_tr(model->config.language, "录制文件已删除", "Recording file deleted"));
    } else {
        cf_app_show_toast(model, cf_tr(model->config.language, "删除失败", "Could not delete file"));
    }
}

void cf_ui_draw_recording_page(struct nk_context *context, CfAppModel *model)
{
    CfTaskState state = cf_controller_state(model->controller);
    bool active = active_state(state);
    bool recording = active && model->activity == CF_ACTIVITY_RECORDING;
    char summary[128];
    double value;
    size_t action_count = 0;
    uint64_t elapsed_ms = 0;
    CfLanguage language = model->config.language;

    nk_layout_row_dynamic(context, 28.0f, 1);
    nk_label(context, cf_tr(language, "录制名称", "Recording name"), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 34.0f, 1);
    nk_edit_string_zero_terminated(context, NK_EDIT_FIELD,
                                   model->recording_name,
                                   (int)sizeof(model->recording_name),
                                   nk_filter_default);

    nk_layout_row_dynamic(context, 42.0f, 3);
    if (!active) {
        if (cf_ui_primary_button(context, cf_tr(language, "开始录制", "Start recording"), false,
                                 model->config.theme)) {
            CfVirtualScreen screen = cf_app_virtual_screen();
            const char *name = model->recording_name[0] != '\0'
                                   ? model->recording_name
                                   : cf_tr(language, "未命名录制", "Untitled recording");
            if (model->recording_dirty &&
                MessageBoxW(model->window,
                            cf_trw(language,
                                  L"当前录制尚未保存，确定丢弃并开始新录制？",
                                  L"This recording has not been saved. Discard it and start a new one?"),
                            L"ClickFlow", MB_ICONWARNING | MB_OKCANCEL) != IDOK) {
                return;
            }
            copy_macro_name(model->recording_name, name);
            if (cf_controller_start_recording(model->controller, &screen) == CF_OK) {
                cf_macro_free(&model->recording);
                cf_macro_init(&model->recording);
                model->recording_ready = false;
                model->recording_dirty = false;
                model->activity = CF_ACTIVITY_RECORDING;
                cf_app_show_toast(model, cf_tr(language, "录制中：鼠标移动、点击与滚轮将被记录", "Recording mouse movement, clicks, and wheel input"));
            } else {
                cf_app_show_toast(model, cf_tr(language, "无法开始录制", "Could not start recording"));
            }
        }
    } else if (recording) {
        if (state == CF_TASK_PAUSED) {
            if (nk_button_label(context, cf_tr(language, "继续", "Resume"))) {
                cf_controller_resume(model->controller);
            }
        } else if (nk_button_label(context, cf_tr(language, "暂停", "Pause"))) {
            cf_controller_pause(model->controller);
        }
    } else {
        nk_button_label(context, cf_tr(language, "任务运行中", "Task running"));
    }

    if (recording) {
        if (cf_ui_primary_button(context, cf_tr(language, "完成并保留", "Finish and keep"), false,
                                 model->config.theme)) {
            CfResult result = cf_controller_finish_recording(
                model->controller, model->recording_name, &model->recording);
            if (result == CF_OK) {
                model->recording_ready = true;
                model->recording_dirty = true;
                model->recording_options = (CfRunOptions){false, 1, 1.0};
                model->activity = CF_ACTIVITY_NONE;
                cf_app_show_toast(model, cf_tr(language, "录制已完成，可回放或保存", "Recording complete. You can play or save it"));
            } else {
                cf_app_show_toast(model, cf_tr(language, "录制为空或无法完成", "Recording is empty or could not be completed"));
            }
        }
        if (nk_button_label(context, cf_tr(language, "取消", "Cancel"))) {
            cf_controller_stop(model->controller);
            model->activity = CF_ACTIVITY_NONE;
            cf_app_show_toast(model, cf_tr(language, "录制已取消", "Recording cancelled"));
        }
    } else {
        if (nk_button_label(context, cf_tr(language, "载入", "Load"))) load_recording(model);
        if (nk_button_label(context, cf_tr(language, "保存", "Save"))) save_recording(model);
    }

    nk_layout_row_dynamic(context, 24.0f, 1);
    if (recording) {
        CfResult progress = cf_controller_recording_progress(
            model->controller, &action_count, &elapsed_ms);
        if (progress != CF_OK) {
            cf_controller_finish_recording(model->controller,
                                           model->recording_name,
                                           &model->recording);
            model->activity = CF_ACTIVITY_NONE;
            cf_app_show_toast(model, cf_tr(language, "录制发生错误并已安全停止", "Recording stopped safely after an error"));
        }
        snprintf(summary, sizeof(summary),
                 cf_tr(language, "%s · %llu 个动作 · %.1f 秒", "%s · %llu actions · %.1f sec"),
                 state == CF_TASK_PAUSED ? cf_tr(language, "已暂停", "Paused")
                                         : cf_tr(language, "正在录制", "Recording"),
                 (unsigned long long)action_count, elapsed_ms / 1000.0);
        nk_label(context, summary, NK_TEXT_LEFT);
    } else if (model->recording_ready) {
        snprintf(summary, sizeof(summary), cf_tr(language, "已就绪 · %llu 个动作", "Ready · %llu actions"),
                 (unsigned long long)model->recording.actions.count);
        nk_label(context, summary, NK_TEXT_LEFT);
    } else {
        nk_label(context, cf_tr(language, "尚未录制或载入操作", "No recording loaded yet"), NK_TEXT_LEFT);
    }

    if (model->recording_ready && !recording) {
        nk_layout_row_dynamic(context, 28.0f, 1);
        nk_label(context, cf_tr(language, "回放", "Playback"), NK_TEXT_LEFT);
        value = model->recording_options.playback_speed;
        nk_layout_row_dynamic(context, 34.0f, 2);
        if (nk_property_double(context, cf_tr(language, "速度", "Speed"), 0.1, &value, 10.0,
                               0.1, 0.05f)) {
            model->recording_options.playback_speed = value;
        }
        value = model->recording_options.repeat_count;
        if (nk_property_double(context, cf_tr(language, "次数", "Repeats"), 1.0, &value,
                               CF_MACRO_REPEAT_MAX, 1.0, 0.25f)) {
            model->recording_options.repeat_count = (uint32_t)value;
        }
        nk_layout_row_dynamic(context, 42.0f, 2);
        if (!active) {
            if (cf_ui_primary_button(context, cf_tr(language, "开始回放", "Start playback"), false,
                                     model->config.theme)) {
                if (cf_controller_start_macro(model->controller,
                                              &model->recording,
                                              &model->recording_options) == CF_OK) {
                    model->activity = CF_ACTIVITY_RECORDING_PLAYBACK;
                    cf_app_show_toast(model, cf_tr(language, "正在回放，按 Esc 停止", "Playing. Press Esc to stop"));
                } else {
                    cf_app_show_toast(model, cf_tr(language, "屏幕布局已变化或录制无效", "Display layout changed or the recording is invalid"));
                }
            }
        } else if (model->activity == CF_ACTIVITY_RECORDING_PLAYBACK) {
            if (cf_ui_primary_button(context, cf_tr(language, "停止回放", "Stop playback"), true,
                                     model->config.theme)) {
                cf_controller_stop(model->controller);
            }
        } else {
            nk_button_label(context, cf_tr(language, "任务运行中", "Task running"));
        }
        if (nk_button_label(context, cf_tr(language, "删除录制文件…", "Delete recording file…"))) {
            delete_recording_file(model);
        }

        nk_layout_row_dynamic(context, 36.0f, 2);
        if (nk_button_label(context, cf_tr(language, "应用新名称", "Apply new name"))) {
            if (cf_macro_set_name(&model->recording,
                                  model->recording_name) == CF_OK) {
                model->recording_dirty = true;
                cf_app_show_toast(model, cf_tr(language, "名称已更新，保存后写入文件", "Name updated. Save to write it to the file"));
            } else {
                cf_app_show_toast(model, cf_tr(language, "名称不能为空且最多 255 字节", "Name is required and must be at most 255 bytes"));
            }
        }
        nk_spacing(context, 1);
    }
}
