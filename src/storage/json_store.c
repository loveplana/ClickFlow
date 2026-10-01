#include "storage/json_store.h"

#include "cJSON.h"

#include <windows.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error != NULL && error_size != 0) {
        snprintf(error, error_size, "%s", message != NULL ? message : "");
    }
}

static CfResult read_file(const wchar_t *path, uint64_t max_bytes,
                          char **content, size_t *length,
                          char *error, size_t error_size)
{
    HANDLE file;
    LARGE_INTEGER size;
    DWORD read_count = 0;
    char *buffer;

    if (path == NULL || content == NULL || length == NULL || max_bytes == 0) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        set_error(error, error_size, "无法打开文件");
        return CF_ERR_IO;
    }
    if (!GetFileSizeEx(file, &size)) {
        CloseHandle(file);
        set_error(error, error_size, "无法读取文件大小");
        return CF_ERR_IO;
    }
    if (size.QuadPart < 0 || (uint64_t)size.QuadPart > max_bytes ||
        (uint64_t)size.QuadPart > SIZE_MAX - 1U) {
        CloseHandle(file);
        set_error(error, error_size, "文件超过大小限制");
        return CF_ERR_LIMIT;
    }

    buffer = malloc((size_t)size.QuadPart + 1U);
    if (buffer == NULL) {
        CloseHandle(file);
        return CF_ERR_OUT_OF_MEMORY;
    }
    if (size.QuadPart != 0 &&
        (!ReadFile(file, buffer, (DWORD)size.QuadPart, &read_count, NULL) ||
         read_count != (DWORD)size.QuadPart)) {
        free(buffer);
        CloseHandle(file);
        set_error(error, error_size, "读取文件失败");
        return CF_ERR_IO;
    }
    CloseHandle(file);
    buffer[(size_t)size.QuadPart] = '\0';
    *content = buffer;
    *length = (size_t)size.QuadPart;
    return CF_OK;
}

static CfResult write_file_atomic(const wchar_t *path, const char *content,
                                  size_t length, char *error,
                                  size_t error_size)
{
    size_t path_length;
    wchar_t *temporary;
    HANDLE file;
    DWORD written = 0;
    bool target_exists;
    bool replaced;

    if (path == NULL || content == NULL || length > UINT32_MAX) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    path_length = wcslen(path);
    temporary = malloc((path_length + 5U) * sizeof(*temporary));
    if (temporary == NULL) {
        return CF_ERR_OUT_OF_MEMORY;
    }
    memcpy(temporary, path, path_length * sizeof(*temporary));
    memcpy(temporary + path_length, L".tmp", 5U * sizeof(*temporary));

    file = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        free(temporary);
        set_error(error, error_size, "无法创建临时文件");
        return CF_ERR_IO;
    }
    if ((length != 0 &&
         (!WriteFile(file, content, (DWORD)length, &written, NULL) ||
          written != (DWORD)length)) ||
        !FlushFileBuffers(file)) {
        CloseHandle(file);
        DeleteFileW(temporary);
        free(temporary);
        set_error(error, error_size, "写入临时文件失败");
        return CF_ERR_IO;
    }
    CloseHandle(file);

    target_exists = GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
    if (target_exists) {
        replaced = ReplaceFileW(path, temporary, NULL, REPLACEFILE_WRITE_THROUGH,
                                NULL, NULL) != 0;
    } else {
        replaced = MoveFileExW(temporary, path,
                               MOVEFILE_REPLACE_EXISTING |
                                   MOVEFILE_WRITE_THROUGH) != 0;
    }
    if (!replaced) {
        DeleteFileW(temporary);
        free(temporary);
        set_error(error, error_size, "替换目标文件失败");
        return CF_ERR_IO;
    }

    free(temporary);
    set_error(error, error_size, "");
    return CF_OK;
}

static cJSON *unique_item(const cJSON *object, const char *name,
                          CfResult *result)
{
    cJSON *item;
    cJSON *match = NULL;
    size_t count = 0;

    if (!cJSON_IsObject(object)) {
        *result = CF_ERR_FORMAT;
        return NULL;
    }
    cJSON_ArrayForEach(item, object) {
        if (item->string != NULL && strcmp(item->string, name) == 0) {
            match = item;
            ++count;
        }
    }
    if (count != 1) {
        *result = CF_ERR_FORMAT;
        return NULL;
    }
    return match;
}

static cJSON *optional_unique_item(const cJSON *object, const char *name,
                                   CfResult *result)
{
    cJSON *item;
    cJSON *match = NULL;
    size_t count = 0;

    if (!cJSON_IsObject(object)) {
        *result = CF_ERR_FORMAT;
        return NULL;
    }
    cJSON_ArrayForEach(item, object) {
        if (item->string != NULL && strcmp(item->string, name) == 0) {
            match = item;
            ++count;
        }
    }
    if (count > 1) {
        *result = CF_ERR_FORMAT;
        return NULL;
    }
    return match;
}

static CfResult parse_hotkey(const cJSON *object, CfHotkey *hotkey);

static CfResult number_u32(const cJSON *item, uint32_t maximum,
                           uint32_t *value)
{
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) ||
        item->valuedouble < 0 || item->valuedouble > maximum ||
        floor(item->valuedouble) != item->valuedouble) {
        return CF_ERR_FORMAT;
    }
    *value = (uint32_t)item->valuedouble;
    return CF_OK;
}

static CfResult number_u64(const cJSON *item, uint64_t maximum,
                           uint64_t *value)
{
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) ||
        item->valuedouble < 0 || item->valuedouble > (double)maximum ||
        floor(item->valuedouble) != item->valuedouble) {
        return CF_ERR_FORMAT;
    }
    *value = (uint64_t)item->valuedouble;
    return CF_OK;
}

static CfResult number_i32(const cJSON *item, int32_t *value)
{
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) ||
        item->valuedouble < INT32_MIN || item->valuedouble > INT32_MAX ||
        floor(item->valuedouble) != item->valuedouble) {
        return CF_ERR_FORMAT;
    }
    *value = (int32_t)item->valuedouble;
    return CF_OK;
}

static CfResult required_u32(const cJSON *object, const char *name,
                             uint32_t maximum, uint32_t *value)
{
    CfResult result = CF_OK;
    cJSON *item = unique_item(object, name, &result);

    return result == CF_OK ? number_u32(item, maximum, value) : result;
}

static CfResult required_i32(const cJSON *object, const char *name,
                             int32_t *value)
{
    CfResult result = CF_OK;
    cJSON *item = unique_item(object, name, &result);

    return result == CF_OK ? number_i32(item, value) : result;
}

static CfResult parse_schema(const cJSON *root)
{
    uint32_t version;
    CfResult result = required_u32(root, "schema_version", UINT32_MAX,
                                   &version);

    if (result != CF_OK) {
        return result;
    }
    return version == CF_JSON_SCHEMA_VERSION ? CF_OK : CF_ERR_SCHEMA;
}

static const char *button_name(CfMouseButton button)
{
    switch (button) {
    case CF_MOUSE_LEFT:
        return "left";
    case CF_MOUSE_MIDDLE:
        return "middle";
    case CF_MOUSE_RIGHT:
        return "right";
    }
    return NULL;
}

static CfResult parse_button(const cJSON *item, CfMouseButton *button)
{
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return CF_ERR_FORMAT;
    }
    if (strcmp(item->valuestring, "left") == 0) {
        *button = CF_MOUSE_LEFT;
    } else if (strcmp(item->valuestring, "middle") == 0) {
        *button = CF_MOUSE_MIDDLE;
    } else if (strcmp(item->valuestring, "right") == 0) {
        *button = CF_MOUSE_RIGHT;
    } else {
        return CF_ERR_FORMAT;
    }
    return CF_OK;
}

static CfResult parse_screen(const cJSON *object, CfVirtualScreen *screen)
{
    CfResult result;

    result = required_i32(object, "left", &screen->left);
    if (result == CF_OK) result = required_i32(object, "top", &screen->top);
    if (result == CF_OK) result = required_i32(object, "width", &screen->width);
    if (result == CF_OK) result = required_i32(object, "height", &screen->height);
    return result;
}

static CfResult parse_action(const cJSON *object, CfAction *action)
{
    CfResult result = CF_OK;
    cJSON *type = unique_item(object, "type", &result);
    cJSON *item;

    memset(action, 0, sizeof(*action));
    if (result != CF_OK || !cJSON_IsString(type) || type->valuestring == NULL) {
        return CF_ERR_FORMAT;
    }
    result = required_u32(object, "delay_ms", CF_ACTION_DELAY_MAX_MS,
                          &action->delay_ms);
    if (result != CF_OK) {
        return result;
    }

    if (strcmp(type->valuestring, "move") == 0) {
        action->type = CF_ACTION_MOVE;
        result = required_i32(object, "x", &action->x);
        if (result == CF_OK) result = required_i32(object, "y", &action->y);
    } else if (strcmp(type->valuestring, "button_down") == 0 ||
               strcmp(type->valuestring, "button_up") == 0 ||
               strcmp(type->valuestring, "click") == 0) {
        action->type = strcmp(type->valuestring, "button_down") == 0
                           ? CF_ACTION_BUTTON_DOWN
                           : strcmp(type->valuestring, "button_up") == 0
                                 ? CF_ACTION_BUTTON_UP
                                 : CF_ACTION_CLICK;
        item = unique_item(object, "button", &result);
        if (result == CF_OK) result = parse_button(item, &action->button);
    } else if (strcmp(type->valuestring, "wheel") == 0) {
        action->type = CF_ACTION_WHEEL;
        result = required_i32(object, "delta", &action->wheel_delta);
    } else if (strcmp(type->valuestring, "wait") == 0) {
        action->type = CF_ACTION_WAIT;
    } else {
        result = CF_ERR_FORMAT;
    }
    return result;
}

static CfResult parse_macro_root(const cJSON *root, const char *expected_kind,
                                 const CfJsonLimits *limits, CfMacro *out)
{
    CfResult result = parse_schema(root);
    cJSON *item;
    cJSON *actions;
    CfMacro parsed;
    int action_count;
    int index;

    if (result != CF_OK) {
        return result;
    }
    item = unique_item(root, "kind", &result);
    if (result != CF_OK || !cJSON_IsString(item) || item->valuestring == NULL ||
        strcmp(item->valuestring, expected_kind) != 0) {
        return CF_ERR_FORMAT;
    }

    cf_macro_init(&parsed);
    item = unique_item(root, "name", &result);
    if (result != CF_OK || !cJSON_IsString(item) || item->valuestring == NULL) {
        result = CF_ERR_FORMAT;
        goto cleanup;
    }
    result = cf_macro_set_name(&parsed, item->valuestring);
    if (result != CF_OK) goto cleanup;
    result = required_u32(root, "repeat_count", CF_MACRO_REPEAT_MAX,
                          &parsed.repeat_count);
    if (result != CF_OK) goto cleanup;

    item = unique_item(root, "playback_speed", &result);
    if (result != CF_OK || !cJSON_IsNumber(item) ||
        !isfinite(item->valuedouble) || item->valuedouble < 0.1 ||
        item->valuedouble > 10.0) {
        result = CF_ERR_FORMAT;
        goto cleanup;
    }
    parsed.playback_speed = item->valuedouble;

    item = unique_item(root, "recorded_virtual_screen", &result);
    if (result != CF_OK || !cJSON_IsObject(item)) {
        result = CF_ERR_FORMAT;
        goto cleanup;
    }
    result = parse_screen(item, &parsed.recorded_screen);
    if (result != CF_OK) goto cleanup;

    item = optional_unique_item(root, "hotkey", &result);
    if (result != CF_OK) goto cleanup;
    if (item != NULL) {
        result = parse_hotkey(item, &parsed.hotkey);
        if (result != CF_OK) goto cleanup;
        parsed.has_hotkey = true;
    }

    actions = unique_item(root, "actions", &result);
    if (result != CF_OK || !cJSON_IsArray(actions)) {
        result = CF_ERR_FORMAT;
        goto cleanup;
    }
    action_count = cJSON_GetArraySize(actions);
    if (action_count < 0 || (size_t)action_count > limits->max_actions) {
        result = CF_ERR_LIMIT;
        goto cleanup;
    }
    for (index = 0; index < action_count; ++index) {
        CfAction action;
        item = cJSON_GetArrayItem(actions, index);
        result = parse_action(item, &action);
        if (result != CF_OK) goto cleanup;
        result = cf_action_list_push(&parsed.actions, action);
        if (result != CF_OK) goto cleanup;
    }
    result = cf_macro_validate(&parsed, &parsed.recorded_screen);
    if (result != CF_OK) goto cleanup;

    cf_macro_free(out);
    *out = parsed;
    return CF_OK;

cleanup:
    cf_macro_free(&parsed);
    return result;
}

CfResult cf_json_load_macro_limited(const wchar_t *path,
                                    const char *expected_kind,
                                    const CfJsonLimits *limits,
                                    CfMacro *out,
                                    char *error, size_t error_size)
{
    char *content = NULL;
    size_t length = 0;
    cJSON *root;
    CfResult result;

    if (expected_kind == NULL || limits == NULL || out == NULL ||
        limits->max_file_bytes == 0 || limits->max_actions == 0 ||
        limits->max_actions > CF_ACTION_LIMIT) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    result = read_file(path, limits->max_file_bytes, &content, &length,
                       error, error_size);
    if (result != CF_OK) {
        return result;
    }
    root = cJSON_ParseWithLength(content, length);
    free(content);
    if (root == NULL) {
        set_error(error, error_size, "JSON 格式无效");
        return CF_ERR_FORMAT;
    }
    result = parse_macro_root(root, expected_kind, limits, out);
    cJSON_Delete(root);
    set_error(error, error_size,
              result == CF_OK ? "" : "宏或录制文件内容无效");
    return result;
}

CfResult cf_json_load_macro(const wchar_t *path, const char *expected_kind,
                            CfMacro *out, char *error, size_t error_size)
{
    const CfJsonLimits limits = {CF_JSON_MAX_FILE_BYTES, CF_ACTION_LIMIT};
    return cf_json_load_macro_limited(path, expected_kind, &limits, out,
                                      error, error_size);
}

static bool add_number(cJSON *object, const char *name, double value)
{
    return cJSON_AddNumberToObject(object, name, value) != NULL;
}

static bool add_string(cJSON *object, const char *name, const char *value)
{
    return value != NULL && cJSON_AddStringToObject(object, name, value) != NULL;
}

static cJSON *action_json(const CfAction *action)
{
    static const char *types[] = {
        "move", "button_down", "button_up", "click", "wheel", "wait"
    };
    cJSON *object = cJSON_CreateObject();
    const char *button;

    if (object == NULL || !add_string(object, "type", types[action->type]) ||
        !add_number(object, "delay_ms", action->delay_ms)) {
        cJSON_Delete(object);
        return NULL;
    }
    if (action->type == CF_ACTION_MOVE &&
        (!add_number(object, "x", action->x) ||
         !add_number(object, "y", action->y))) {
        cJSON_Delete(object);
        return NULL;
    }
    if (action->type == CF_ACTION_BUTTON_DOWN ||
        action->type == CF_ACTION_BUTTON_UP ||
        action->type == CF_ACTION_CLICK) {
        button = button_name(action->button);
        if (!add_string(object, "button", button)) {
            cJSON_Delete(object);
            return NULL;
        }
    }
    if (action->type == CF_ACTION_WHEEL &&
        !add_number(object, "delta", action->wheel_delta)) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

CfResult cf_json_save_macro_atomic(const wchar_t *path, const char *kind,
                                   const CfMacro *macro,
                                   char *error, size_t error_size)
{
    cJSON *root = NULL;
    cJSON *screen = NULL;
    cJSON *actions = NULL;
    cJSON *hotkey = NULL;
    char *text = NULL;
    CfResult result;
    size_t index;

    if (kind == NULL || (strcmp(kind, "macro") != 0 &&
                         strcmp(kind, "recording") != 0) || macro == NULL ||
        cf_macro_validate(macro, &macro->recorded_screen) != CF_OK) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    root = cJSON_CreateObject();
    screen = cJSON_CreateObject();
    actions = cJSON_CreateArray();
    if (root == NULL || screen == NULL || actions == NULL ||
        !add_number(root, "schema_version", CF_JSON_SCHEMA_VERSION) ||
        !add_string(root, "kind", kind) ||
        !add_string(root, "name", macro->name) ||
        !add_number(root, "repeat_count", macro->repeat_count) ||
        !add_number(root, "playback_speed", macro->playback_speed) ||
        !add_number(screen, "left", macro->recorded_screen.left) ||
        !add_number(screen, "top", macro->recorded_screen.top) ||
        !add_number(screen, "width", macro->recorded_screen.width) ||
        !add_number(screen, "height", macro->recorded_screen.height)) {
        cJSON_Delete(screen);
        cJSON_Delete(actions);
        cJSON_Delete(root);
        return CF_ERR_OUT_OF_MEMORY;
    }
    cJSON_AddItemToObject(root, "recorded_virtual_screen", screen);
    cJSON_AddItemToObject(root, "actions", actions);
    screen = NULL;
    actions = NULL;
    if (macro->has_hotkey) {
        hotkey = cJSON_CreateObject();
        if (hotkey == NULL ||
            !add_number(hotkey, "modifiers", macro->hotkey.modifiers) ||
            !add_number(hotkey, "virtual_key", macro->hotkey.virtual_key)) {
            cJSON_Delete(hotkey);
            cJSON_Delete(root);
            return CF_ERR_OUT_OF_MEMORY;
        }
        cJSON_AddItemToObject(root, "hotkey", hotkey);
    }
    for (index = 0; index < macro->actions.count; ++index) {
        cJSON *item = action_json(&macro->actions.items[index]);
        if (item == NULL) {
            cJSON_Delete(root);
            return CF_ERR_OUT_OF_MEMORY;
        }
        cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(root, "actions"),
                             item);
    }
    text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (text == NULL) {
        return CF_ERR_OUT_OF_MEMORY;
    }
    result = write_file_atomic(path, text, strlen(text), error, error_size);
    cJSON_free(text);
    return result;
}

static const char *theme_name(CfTheme theme)
{
    return theme == CF_THEME_DARK ? "dark" : "light";
}

static const char *language_name(CfLanguage language)
{
    return language == CF_LANGUAGE_EN_US ? "en-US" : "zh-CN";
}

static const char *click_kind_name(CfClickKind kind)
{
    return kind == CF_CLICK_SINGLE ? "single" : "double";
}

static const char *stop_mode_name(CfStopMode mode)
{
    static const char *names[] = {"manual", "count", "duration"};
    return names[mode];
}

static const char *position_mode_name(CfPositionMode mode)
{
    return mode == CF_POSITION_CURSOR ? "cursor" : "fixed";
}

static CfVirtualScreen validation_screen(const CfClickerConfig *clicker)
{
    if (clicker->position_mode == CF_POSITION_FIXED) {
        return (CfVirtualScreen){clicker->fixed_x, clicker->fixed_y, 1, 1};
    }
    return (CfVirtualScreen){0, 0, 1, 1};
}

CfResult cf_json_save_config_atomic(const wchar_t *path,
                                    const CfConfig *config,
                                    char *error, size_t error_size)
{
    cJSON *root = NULL;
    cJSON *clicker = NULL;
    cJSON *hotkeys = NULL;
    cJSON *primary = NULL;
    cJSON *emergency = NULL;
    char *text;
    CfResult result;
    CfVirtualScreen screen;

    if (config == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    screen = validation_screen(&config->clicker);
    if (cf_config_validate(config, &screen) != CF_OK) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    root = cJSON_CreateObject();
    clicker = cJSON_CreateObject();
    hotkeys = cJSON_CreateObject();
    primary = cJSON_CreateObject();
    emergency = cJSON_CreateObject();
    if (root == NULL || clicker == NULL || hotkeys == NULL || primary == NULL ||
        emergency == NULL ||
        !add_number(root, "schema_version", CF_JSON_SCHEMA_VERSION) ||
        !add_string(root, "theme", theme_name(config->theme)) ||
        !add_string(root, "language", language_name(config->language)) ||
        !add_string(clicker, "button", button_name(config->clicker.button)) ||
        !add_string(clicker, "click_kind",
                    click_kind_name(config->clicker.click_kind)) ||
        !add_string(clicker, "stop_mode",
                    stop_mode_name(config->clicker.stop_mode)) ||
        !add_string(clicker, "position_mode",
                    position_mode_name(config->clicker.position_mode)) ||
        !add_number(clicker, "interval_ms", config->clicker.interval_ms) ||
        !add_number(clicker, "click_count", (double)config->clicker.click_count) ||
        !add_number(clicker, "duration_ms", config->clicker.duration_ms) ||
        !add_number(clicker, "fixed_x", config->clicker.fixed_x) ||
        !add_number(clicker, "fixed_y", config->clicker.fixed_y) ||
        !add_number(primary, "modifiers", config->primary_hotkey.modifiers) ||
        !add_number(primary, "virtual_key", config->primary_hotkey.virtual_key) ||
        !add_number(emergency, "modifiers",
                    config->emergency_hotkey.modifiers) ||
        !add_number(emergency, "virtual_key",
                    config->emergency_hotkey.virtual_key)) {
        cJSON_Delete(clicker);
        cJSON_Delete(hotkeys);
        cJSON_Delete(primary);
        cJSON_Delete(emergency);
        cJSON_Delete(root);
        return CF_ERR_OUT_OF_MEMORY;
    }
    cJSON_AddItemToObject(hotkeys, "primary", primary);
    cJSON_AddItemToObject(hotkeys, "emergency", emergency);
    cJSON_AddItemToObject(root, "clicker", clicker);
    cJSON_AddItemToObject(root, "hotkeys", hotkeys);
    text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (text == NULL) {
        return CF_ERR_OUT_OF_MEMORY;
    }
    result = write_file_atomic(path, text, strlen(text), error, error_size);
    cJSON_free(text);
    return result;
}

static CfResult parse_named_string(const cJSON *object, const char *name,
                                   const char **value)
{
    CfResult result = CF_OK;
    cJSON *item = unique_item(object, name, &result);

    if (result != CF_OK || !cJSON_IsString(item) || item->valuestring == NULL) {
        return CF_ERR_FORMAT;
    }
    *value = item->valuestring;
    return CF_OK;
}

static CfResult parse_hotkey(const cJSON *object, CfHotkey *hotkey)
{
    CfResult result;
    uint32_t modifiers;
    uint32_t virtual_key;

    result = required_u32(object, "modifiers", UINT32_MAX, &modifiers);
    if (result == CF_OK) {
        result = required_u32(object, "virtual_key", UINT32_MAX, &virtual_key);
    }
    if (result != CF_OK) {
        return result;
    }
    hotkey->modifiers = modifiers;
    hotkey->virtual_key = virtual_key;
    return cf_hotkey_validate(hotkey) == CF_OK ? CF_OK : CF_ERR_FORMAT;
}

static CfResult parse_clicker(const cJSON *object, CfClickerConfig *clicker)
{
    const char *text;
    CfResult result;

    result = parse_named_string(object, "button", &text);
    if (result == CF_OK) {
        cJSON temporary = {0};
        temporary.type = cJSON_String;
        temporary.valuestring = (char *)text;
        result = parse_button(&temporary, &clicker->button);
    }
    if (result == CF_OK) result = parse_named_string(object, "click_kind", &text);
    if (result == CF_OK) {
        if (strcmp(text, "single") == 0) clicker->click_kind = CF_CLICK_SINGLE;
        else if (strcmp(text, "double") == 0) clicker->click_kind = CF_CLICK_DOUBLE;
        else result = CF_ERR_FORMAT;
    }
    if (result == CF_OK) result = parse_named_string(object, "stop_mode", &text);
    if (result == CF_OK) {
        if (strcmp(text, "manual") == 0) clicker->stop_mode = CF_STOP_MANUAL;
        else if (strcmp(text, "count") == 0) clicker->stop_mode = CF_STOP_AFTER_COUNT;
        else if (strcmp(text, "duration") == 0) clicker->stop_mode = CF_STOP_AFTER_DURATION;
        else result = CF_ERR_FORMAT;
    }
    if (result == CF_OK) result = parse_named_string(object, "position_mode", &text);
    if (result == CF_OK) {
        if (strcmp(text, "cursor") == 0) clicker->position_mode = CF_POSITION_CURSOR;
        else if (strcmp(text, "fixed") == 0) clicker->position_mode = CF_POSITION_FIXED;
        else result = CF_ERR_FORMAT;
    }
    if (result == CF_OK) result = required_u32(object, "interval_ms", CF_CLICK_INTERVAL_MAX_MS, &clicker->interval_ms);
    if (result == CF_OK) result = number_u64(unique_item(object, "click_count", &result), CF_CLICK_COUNT_MAX, &clicker->click_count);
    if (result == CF_OK) result = required_u32(object, "duration_ms", CF_CLICK_DURATION_MAX_MS, &clicker->duration_ms);
    if (result == CF_OK) result = required_i32(object, "fixed_x", &clicker->fixed_x);
    if (result == CF_OK) result = required_i32(object, "fixed_y", &clicker->fixed_y);
    return result;
}

CfResult cf_json_load_config(const wchar_t *path, CfConfig *out,
                             char *error, size_t error_size)
{
    char *content = NULL;
    size_t length = 0;
    cJSON *root = NULL;
    cJSON *item;
    cJSON *hotkeys;
    const char *theme;
    const char *language;
    CfConfig parsed;
    CfVirtualScreen screen;
    CfResult result;

    if (out == NULL) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    result = read_file(path, CF_JSON_MAX_FILE_BYTES, &content, &length,
                       error, error_size);
    if (result != CF_OK) return result;
    root = cJSON_ParseWithLength(content, length);
    free(content);
    if (root == NULL) {
        set_error(error, error_size, "JSON 格式无效");
        return CF_ERR_FORMAT;
    }
    result = parse_schema(root);
    if (result != CF_OK) goto cleanup;
    cf_config_defaults(&parsed);
    result = parse_named_string(root, "theme", &theme);
    if (result == CF_OK) {
        if (strcmp(theme, "dark") == 0) parsed.theme = CF_THEME_DARK;
        else if (strcmp(theme, "light") == 0) parsed.theme = CF_THEME_LIGHT;
        else result = CF_ERR_FORMAT;
    }
    item = result == CF_OK ? optional_unique_item(root, "language", &result) : NULL;
    if (result == CF_OK && item != NULL) {
        if (!cJSON_IsString(item) || item->valuestring == NULL) {
            result = CF_ERR_FORMAT;
        } else {
            language = item->valuestring;
            if (strcmp(language, "zh-CN") == 0) parsed.language = CF_LANGUAGE_ZH_CN;
            else if (strcmp(language, "en-US") == 0) parsed.language = CF_LANGUAGE_EN_US;
            else result = CF_ERR_FORMAT;
        }
    }
    item = result == CF_OK ? unique_item(root, "clicker", &result) : NULL;
    if (result == CF_OK) result = parse_clicker(item, &parsed.clicker);
    hotkeys = result == CF_OK ? unique_item(root, "hotkeys", &result) : NULL;
    item = result == CF_OK ? unique_item(hotkeys, "primary", &result) : NULL;
    if (result == CF_OK) result = parse_hotkey(item, &parsed.primary_hotkey);
    item = result == CF_OK ? unique_item(hotkeys, "emergency", &result) : NULL;
    if (result == CF_OK) result = parse_hotkey(item, &parsed.emergency_hotkey);
    if (result == CF_OK && cf_hotkey_validate_pair(parsed.primary_hotkey,
                                                    parsed.emergency_hotkey) != CF_OK) {
        result = CF_ERR_FORMAT;
    }
    screen = validation_screen(&parsed.clicker);
    if (result == CF_OK && cf_clicker_validate(&parsed.clicker, &screen) != CF_OK) {
        result = CF_ERR_FORMAT;
    }
    if (result == CF_OK) *out = parsed;

cleanup:
    cJSON_Delete(root);
    set_error(error, error_size,
              result == CF_OK ? "" : "配置文件内容无效");
    return result;
}
