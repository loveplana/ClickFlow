#include "core/config.h"
#include "storage/json_store.h"
#include "test.h"

#include <windows.h>

#include <stdio.h>
#include <string.h>

static void temporary_path(wchar_t *path, DWORD path_count)
{
    wchar_t directory[MAX_PATH];

    CF_TEST_ASSERT(GetTempPathW(MAX_PATH, directory) != 0);
    CF_TEST_ASSERT(GetTempFileNameW(directory, L"cfl", 0, path) != 0);
    CF_TEST_ASSERT(path_count >= MAX_PATH);
}

static void write_utf8(const wchar_t *path, const char *text)
{
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0;
    DWORD length = (DWORD)strlen(text);

    CF_TEST_ASSERT(file != INVALID_HANDLE_VALUE);
    if (file != INVALID_HANDLE_VALUE) {
        CF_TEST_ASSERT(WriteFile(file, text, length, &written, NULL));
        CF_TEST_ASSERT_EQ(written, length);
        CloseHandle(file);
    }
}

static CfVirtualScreen test_screen(void)
{
    CfVirtualScreen screen = {0, 0, 1920, 1080};
    return screen;
}

static void make_macro(CfMacro *macro)
{
    CfAction move = {0};
    CfAction click = {0};
    CfAction wheel = {0};

    cf_macro_init(macro);
    CF_TEST_ASSERT_EQ(cf_macro_set_name(macro, "示例宏"), CF_OK);
    macro->repeat_count = 3;
    macro->playback_speed = 1.25;
    macro->recorded_screen = test_screen();
    macro->has_hotkey = true;
    macro->hotkey = (CfHotkey){CF_HOTKEY_CONTROL, 0x78};

    move.type = CF_ACTION_MOVE;
    move.x = 820;
    move.y = 460;
    click.type = CF_ACTION_CLICK;
    click.button = CF_MOUSE_LEFT;
    click.delay_ms = 120;
    wheel.type = CF_ACTION_WHEEL;
    wheel.wheel_delta = -120;
    wheel.delay_ms = 300;
    CF_TEST_ASSERT_EQ(cf_action_list_push(&macro->actions, move), CF_OK);
    CF_TEST_ASSERT_EQ(cf_action_list_push(&macro->actions, click), CF_OK);
    CF_TEST_ASSERT_EQ(cf_action_list_push(&macro->actions, wheel), CF_OK);
}

static void test_default_config_round_trips(void)
{
    wchar_t path[MAX_PATH];
    char error[256];
    CfConfig expected;
    CfConfig actual;

    temporary_path(path, CF_ARRAY_COUNT(path));
    cf_config_defaults(&expected);
    expected.theme = CF_THEME_LIGHT;
    expected.language = CF_LANGUAGE_EN_US;
    expected.clicker.interval_ms = 37;
    expected.primary_hotkey.modifiers = CF_HOTKEY_CONTROL;

    CF_TEST_ASSERT_EQ(cf_json_save_config_atomic(path, &expected, error,
                                                 sizeof(error)), CF_OK);
    memset(&actual, 0, sizeof(actual));
    CF_TEST_ASSERT_EQ(cf_json_load_config(path, &actual, error, sizeof(error)),
                      CF_OK);
    CF_TEST_ASSERT_EQ(actual.theme, CF_THEME_LIGHT);
    CF_TEST_ASSERT_EQ(actual.language, CF_LANGUAGE_EN_US);
    CF_TEST_ASSERT_EQ(actual.clicker.interval_ms, 37);
    CF_TEST_ASSERT_EQ(actual.primary_hotkey.modifiers, CF_HOTKEY_CONTROL);
    CF_TEST_ASSERT_EQ(actual.primary_hotkey.virtual_key,
                      expected.primary_hotkey.virtual_key);
    DeleteFileW(path);
}

static void test_legacy_config_defaults_to_chinese(void)
{
    static const char json[] =
        "{\"schema_version\":1,\"theme\":\"dark\","
        "\"clicker\":{\"button\":\"left\",\"click_kind\":\"single\","
        "\"stop_mode\":\"manual\",\"position_mode\":\"cursor\","
        "\"interval_ms\":100,\"click_count\":100,\"duration_ms\":10000,"
        "\"fixed_x\":0,\"fixed_y\":0},"
        "\"hotkeys\":{\"primary\":{\"modifiers\":0,\"virtual_key\":119},"
        "\"emergency\":{\"modifiers\":0,\"virtual_key\":27}}}";
    wchar_t path[MAX_PATH];
    char error[256];
    CfConfig config;

    temporary_path(path, CF_ARRAY_COUNT(path));
    write_utf8(path, json);
    CF_TEST_ASSERT_EQ(cf_json_load_config(path, &config, error, sizeof(error)),
                      CF_OK);
    CF_TEST_ASSERT_EQ(config.language, CF_LANGUAGE_ZH_CN);
    DeleteFileW(path);
}

static void test_macro_round_trips_utf8_and_actions(void)
{
    wchar_t path[MAX_PATH];
    char error[256];
    CfMacro expected;
    CfMacro actual;

    temporary_path(path, CF_ARRAY_COUNT(path));
    make_macro(&expected);
    cf_macro_init(&actual);
    CF_TEST_ASSERT_EQ(cf_json_save_macro_atomic(path, "macro", &expected,
                                                error, sizeof(error)), CF_OK);
    CF_TEST_ASSERT_EQ(cf_json_load_macro(path, "macro", &actual, error,
                                         sizeof(error)), CF_OK);
    CF_TEST_ASSERT(strcmp(actual.name, "示例宏") == 0);
    CF_TEST_ASSERT_EQ(actual.repeat_count, 3);
    CF_TEST_ASSERT(actual.playback_speed == 1.25);
    CF_TEST_ASSERT_EQ(actual.actions.count, 3);
    CF_TEST_ASSERT_EQ(actual.actions.items[0].x, 820);
    CF_TEST_ASSERT_EQ(actual.actions.items[1].button, CF_MOUSE_LEFT);
    CF_TEST_ASSERT_EQ(actual.actions.items[2].wheel_delta, -120);
    CF_TEST_ASSERT(actual.has_hotkey);
    CF_TEST_ASSERT_EQ(actual.hotkey.modifiers, CF_HOTKEY_CONTROL);
    CF_TEST_ASSERT_EQ(actual.hotkey.virtual_key, 0x78);
    cf_macro_free(&actual);
    cf_macro_free(&expected);
    DeleteFileW(path);
}

static void test_loads_design_schema_example(void)
{
    static const char json[] =
        "{\"schema_version\":1,\"kind\":\"macro\",\"name\":\"示例宏\"," 
        "\"repeat_count\":3,\"playback_speed\":1.0,"
        "\"recorded_virtual_screen\":{\"left\":0,\"top\":0,"
        "\"width\":1920,\"height\":1080},\"actions\":["
        "{\"type\":\"move\",\"x\":820,\"y\":460,\"delay_ms\":0},"
        "{\"type\":\"click\",\"button\":\"left\",\"delay_ms\":120},"
        "{\"type\":\"wheel\",\"delta\":-120,\"delay_ms\":300}]}";
    wchar_t path[MAX_PATH];
    char error[256];
    CfMacro macro;

    temporary_path(path, CF_ARRAY_COUNT(path));
    write_utf8(path, json);
    cf_macro_init(&macro);
    CF_TEST_ASSERT_EQ(cf_json_load_macro(path, "macro", &macro, error,
                                         sizeof(error)), CF_OK);
    CF_TEST_ASSERT_EQ(macro.actions.count, 3);
    cf_macro_free(&macro);
    DeleteFileW(path);
}

static void test_rejects_schema_kind_and_truncated_json(void)
{
    wchar_t path[MAX_PATH];
    char error[256];
    CfMacro macro;

    temporary_path(path, CF_ARRAY_COUNT(path));
    cf_macro_init(&macro);
    write_utf8(path, "{\"schema_version\":2}");
    CF_TEST_ASSERT_EQ(cf_json_load_macro(path, "macro", &macro, error,
                                         sizeof(error)), CF_ERR_SCHEMA);
    write_utf8(path, "{\"schema_version\":1,\"kind\":\"recording\"}");
    CF_TEST_ASSERT_EQ(cf_json_load_macro(path, "macro", &macro, error,
                                         sizeof(error)), CF_ERR_FORMAT);
    write_utf8(path, "{\"schema_version\":1");
    CF_TEST_ASSERT_EQ(cf_json_load_macro(path, "macro", &macro, error,
                                         sizeof(error)), CF_ERR_FORMAT);
    cf_macro_free(&macro);
    DeleteFileW(path);
}

static void test_failed_load_preserves_existing_macro(void)
{
    wchar_t path[MAX_PATH];
    char error[256];
    CfMacro macro;

    temporary_path(path, CF_ARRAY_COUNT(path));
    make_macro(&macro);
    write_utf8(path, "not-json");
    CF_TEST_ASSERT_EQ(cf_json_load_macro(path, "macro", &macro, error,
                                         sizeof(error)), CF_ERR_FORMAT);
    CF_TEST_ASSERT(strcmp(macro.name, "示例宏") == 0);
    CF_TEST_ASSERT_EQ(macro.actions.count, 3);
    cf_macro_free(&macro);
    DeleteFileW(path);
}

static void test_rejects_file_above_64_mib_before_reading(void)
{
    wchar_t path[MAX_PATH];
    char error[256];
    CfMacro macro;
    LARGE_INTEGER size;
    HANDLE file;

    temporary_path(path, CF_ARRAY_COUNT(path));
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    CF_TEST_ASSERT(file != INVALID_HANDLE_VALUE);
    size.QuadPart = (LONGLONG)CF_JSON_MAX_FILE_BYTES + 1;
    CF_TEST_ASSERT(SetFilePointerEx(file, size, NULL, FILE_BEGIN));
    CF_TEST_ASSERT(SetEndOfFile(file));
    CloseHandle(file);

    cf_macro_init(&macro);
    CF_TEST_ASSERT_EQ(cf_json_load_macro(path, "macro", &macro, error,
                                         sizeof(error)), CF_ERR_LIMIT);
    cf_macro_free(&macro);
    DeleteFileW(path);
}

static void test_rejects_action_count_above_injected_limit(void)
{
    static const char json[] =
        "{\"schema_version\":1,\"kind\":\"macro\",\"name\":\"x\"," 
        "\"repeat_count\":1,\"playback_speed\":1.0,"
        "\"recorded_virtual_screen\":{\"left\":0,\"top\":0,"
        "\"width\":1920,\"height\":1080},\"actions\":["
        "{\"type\":\"wait\",\"delay_ms\":1},"
        "{\"type\":\"wait\",\"delay_ms\":1},"
        "{\"type\":\"wait\",\"delay_ms\":1}]}";
    CfJsonLimits limits = {CF_JSON_MAX_FILE_BYTES, 2};
    wchar_t path[MAX_PATH];
    char error[256];
    CfMacro macro;

    temporary_path(path, CF_ARRAY_COUNT(path));
    write_utf8(path, json);
    cf_macro_init(&macro);
    CF_TEST_ASSERT_EQ(cf_json_load_macro_limited(path, "macro", &limits,
                                                 &macro, error, sizeof(error)),
                      CF_ERR_LIMIT);
    cf_macro_free(&macro);
    DeleteFileW(path);
}

static void test_loaded_layout_is_validated_against_current_screen(void)
{
    wchar_t path[MAX_PATH];
    char error[256];
    CfMacro source;
    CfMacro loaded;
    CfVirtualScreen changed = {0, 0, 2560, 1440};

    temporary_path(path, CF_ARRAY_COUNT(path));
    make_macro(&source);
    cf_macro_init(&loaded);
    CF_TEST_ASSERT_EQ(cf_json_save_macro_atomic(path, "recording", &source,
                                                error, sizeof(error)), CF_OK);
    CF_TEST_ASSERT_EQ(cf_json_load_macro(path, "recording", &loaded, error,
                                         sizeof(error)), CF_OK);
    CF_TEST_ASSERT_EQ(cf_macro_validate(&loaded, &changed), CF_ERR_CONFLICT);
    cf_macro_free(&loaded);
    cf_macro_free(&source);
    DeleteFileW(path);
}

static void test_save_to_missing_directory_reports_io(void)
{
    wchar_t path[MAX_PATH];
    char error[256];
    CfConfig config;

    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"clickflow-directory-that-does-not-exist\\config.json");
    cf_config_defaults(&config);
    CF_TEST_ASSERT_EQ(cf_json_save_config_atomic(path, &config, error,
                                                 sizeof(error)), CF_ERR_IO);
}

void cf_test_storage(void)
{
    test_default_config_round_trips();
    test_legacy_config_defaults_to_chinese();
    test_macro_round_trips_utf8_and_actions();
    test_loads_design_schema_example();
    test_rejects_schema_kind_and_truncated_json();
    test_failed_load_preserves_existing_macro();
    test_rejects_file_above_64_mib_before_reading();
    test_rejects_action_count_above_injected_limit();
    test_loaded_layout_is_validated_against_current_screen();
    test_save_to_missing_directory_reports_io();
}
