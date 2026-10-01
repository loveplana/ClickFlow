#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_D3D11_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_d3d11.h"

#include "platform/win32_input.h"
#include "platform/win32_hotkey.h"
#include "platform/win32_paths.h"
#include "platform/win32_single_instance.h"
#include "storage/json_store.h"
#include "ui/app.h"
#include "ui/i18n.h"
#include "ui/pages.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#define CF_WINDOW_CLASS L"ClickFlowWindowClass"
#define CF_WINDOW_TITLE L"ClickFlow"
#define CF_INITIAL_WIDTH 960
#define CF_INITIAL_HEIGHT 680
#define CF_MIN_WIDTH 820
#define CF_MIN_HEIGHT 580
#define CF_NAV_WIDTH 176
#define CF_SHELL_GUTTER 56
#define CF_MAX_VERTEX_BUFFER (512U * 1024U)
#define CF_MAX_INDEX_BUFFER (128U * 1024U)
#define CF_CONTROLLER_MESSAGE (WM_APP + 1U)

enum {
    CF_HOTKEY_PRIMARY_ID = 1,
    CF_HOTKEY_EMERGENCY_ID = 2,
    CF_HOTKEY_MACRO_ID = 3
};

typedef struct CfApp {
    HINSTANCE instance;
    HWND window;
    IDXGISwapChain *swap_chain;
    ID3D11Device *device;
    ID3D11DeviceContext *device_context;
    ID3D11RenderTargetView *render_target;
    struct nk_context *ui;
    int width;
    int height;
    bool running;
    CfAppModel model;
    CfWin32Input input;
    CfSingleInstance single_instance;
    bool hotkeys_registered;
} CfApp;

static CfApp *current_app;

CfVirtualScreen cf_app_virtual_screen(void)
{
    return (CfVirtualScreen){GetSystemMetrics(SM_XVIRTUALSCREEN),
                             GetSystemMetrics(SM_YVIRTUALSCREEN),
                             GetSystemMetrics(SM_CXVIRTUALSCREEN),
                             GetSystemMetrics(SM_CYVIRTUALSCREEN)};
}

void cf_app_show_toast(CfAppModel *model, const char *message)
{
    if (model == NULL) return;
    snprintf(model->toast, sizeof(model->toast), "%s",
             message != NULL ? message : "");
    model->toast_until_ms = GetTickCount64() + 3500U;
}

void cf_app_set_language(CfAppModel *model, CfLanguage language)
{
    const char *macro_name;
    const char *recording_name;

    if (model == NULL || model->config.language == language) return;
    macro_name = cf_tr(language, "新建宏", "New macro");
    recording_name = cf_tr(language, "未命名录制", "Untitled recording");
    if (model->macro.actions.count == 0 &&
        (strcmp(model->macro_name, "新建宏") == 0 ||
         strcmp(model->macro_name, "New macro") == 0)) {
        cf_macro_set_name(&model->macro, macro_name);
        snprintf(model->macro_name, sizeof(model->macro_name), "%s", macro_name);
    }
    if (!model->recording_ready &&
        (strcmp(model->recording_name, "未命名录制") == 0 ||
         strcmp(model->recording_name, "Untitled recording") == 0)) {
        snprintf(model->recording_name, sizeof(model->recording_name), "%s",
                 recording_name);
    }
    model->config.language = language;
    model->config_dirty = true;
    model->toast[0] = '\0';
}

static bool config_path(wchar_t *path, size_t count)
{
    size_t used;

    if (cf_win32_data_directory(path, count) != CF_OK) return false;
    used = wcslen(path);
    if (used + wcslen(L"\\config.json") + 1U > count) return false;
    wcscat(path, L"\\config.json");
    return true;
}

static void back_up_invalid_config(const wchar_t *path)
{
    wchar_t directory[MAX_PATH];
    wchar_t backup[MAX_PATH];
    SYSTEMTIME time;

    if (path == NULL || cf_win32_data_directory(directory,
                                                 CF_ARRAY_COUNT(directory)) != CF_OK) {
        return;
    }
    GetLocalTime(&time);
    if (swprintf(backup, CF_ARRAY_COUNT(backup),
                 L"%ls\\backups\\config-%04u%02u%02u-%02u%02u%02u.json",
                 directory, (unsigned)time.wYear, (unsigned)time.wMonth,
                 (unsigned)time.wDay, (unsigned)time.wHour,
                 (unsigned)time.wMinute, (unsigned)time.wSecond) < 0) {
        return;
    }
    MoveFileExW(path, backup, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

static void load_config(CfAppModel *model)
{
    wchar_t path[MAX_PATH];
    char error[160];

    cf_config_defaults(&model->config);
    if (!config_path(path, CF_ARRAY_COUNT(path)) ||
        GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
        return;
    }
    if (cf_json_load_config(path, &model->config,
                            error, sizeof(error)) != CF_OK) {
        back_up_invalid_config(path);
        cf_config_defaults(&model->config);
        cf_app_show_toast(model, cf_tr(model->config.language,
                          "配置文件无效，已备份并恢复默认设置",
                          "Invalid configuration was backed up; defaults restored"));
    }
}

static void unregister_hotkeys(CfApp *app)
{
    if (app == NULL || app->window == NULL) return;
    cf_win32_hotkey_unregister(app->window, CF_HOTKEY_PRIMARY_ID);
    cf_win32_hotkey_unregister(app->window, CF_HOTKEY_EMERGENCY_ID);
    cf_win32_hotkey_unregister(app->window, CF_HOTKEY_MACRO_ID);
    app->hotkeys_registered = false;
}

static CfResult register_hotkeys(CfApp *app)
{
    CfResult result;
    CfAppModel *model;

    if (app == NULL || app->window == NULL) return CF_ERR_INVALID_ARGUMENT;
    model = &app->model;
    unregister_hotkeys(app);
    if (model->macro.has_hotkey &&
        (cf_hotkey_equal(model->macro.hotkey, model->config.primary_hotkey) ||
         cf_hotkey_equal(model->macro.hotkey, model->config.emergency_hotkey))) {
        return CF_ERR_CONFLICT;
    }
    result = cf_win32_hotkey_register(app->window, CF_HOTKEY_PRIMARY_ID,
                                      model->config.primary_hotkey);
    if (result != CF_OK) return result;
    result = cf_win32_hotkey_register(app->window, CF_HOTKEY_EMERGENCY_ID,
                                      model->config.emergency_hotkey);
    if (result != CF_OK) {
        cf_win32_hotkey_unregister(app->window, CF_HOTKEY_PRIMARY_ID);
        return result;
    }
    app->hotkeys_registered = true;
    if (model->macro.has_hotkey) {
        result = cf_win32_hotkey_register(app->window, CF_HOTKEY_MACRO_ID,
                                          model->macro.hotkey);
    }
    return result;
}

CfResult cf_app_refresh_hotkeys(CfAppModel *model)
{
    if (current_app == NULL || model != &current_app->model) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    return register_hotkeys(current_app);
}

CfResult cf_app_save_settings(CfAppModel *model)
{
    wchar_t path[MAX_PATH];
    char error[160];
    CfVirtualScreen screen;
    CfResult result;

    if (model == NULL || !config_path(path, CF_ARRAY_COUNT(path))) {
        return CF_ERR_PLATFORM;
    }
    screen = cf_app_virtual_screen();
    result = cf_config_validate(&model->config, &screen);
    if (result != CF_OK) return result;
    result = cf_app_refresh_hotkeys(model);
    if (result != CF_OK) return result;
    result = cf_json_save_config_atomic(path, &model->config,
                                        error, sizeof(error));
    if (result == CF_OK) model->config_dirty = false;
    return result;
}

static bool create_render_target(CfApp *app, int width, int height)
{
    ID3D11Texture2D *back_buffer = NULL;
    D3D11_RENDER_TARGET_VIEW_DESC description;
    HRESULT result;

    if (app->render_target != NULL) {
        ID3D11RenderTargetView_Release(app->render_target);
        app->render_target = NULL;
    }
    ID3D11DeviceContext_OMSetRenderTargets(app->device_context, 0, NULL, NULL);
    result = IDXGISwapChain_ResizeBuffers(app->swap_chain, 0, (UINT)width,
                                         (UINT)height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(result)) return false;

    memset(&description, 0, sizeof(description));
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    result = IDXGISwapChain_GetBuffer(app->swap_chain, 0,
                                     &IID_ID3D11Texture2D,
                                     (void **)&back_buffer);
    if (FAILED(result)) return false;
    result = ID3D11Device_CreateRenderTargetView(
        app->device, (ID3D11Resource *)back_buffer, &description,
        &app->render_target);
    ID3D11Texture2D_Release(back_buffer);
    return SUCCEEDED(result);
}

static bool create_d3d(CfApp *app)
{
    DXGI_SWAP_CHAIN_DESC description;
    D3D_FEATURE_LEVEL feature_level;
    HRESULT result;

    memset(&description, 0, sizeof(description));
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferDesc.RefreshRate.Numerator = 60;
    description.BufferDesc.RefreshRate.Denominator = 1;
    description.SampleDesc.Count = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 2;
    description.OutputWindow = app->window;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    result = D3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION,
        &description, &app->swap_chain, &app->device, &feature_level,
        &app->device_context);
    if (FAILED(result)) {
        result = D3D11CreateDeviceAndSwapChain(
            NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION,
            &description, &app->swap_chain, &app->device, &feature_level,
            &app->device_context);
    }
    return SUCCEEDED(result) &&
           create_render_target(app, app->width, app->height);
}

static void destroy_d3d(CfApp *app)
{
    if (app->device_context != NULL) {
        ID3D11DeviceContext_ClearState(app->device_context);
    }
    if (app->render_target != NULL) ID3D11RenderTargetView_Release(app->render_target);
    if (app->device_context != NULL) ID3D11DeviceContext_Release(app->device_context);
    if (app->device != NULL) ID3D11Device_Release(app->device);
    if (app->swap_chain != NULL) IDXGISwapChain_Release(app->swap_chain);
    app->render_target = NULL;
    app->device_context = NULL;
    app->device = NULL;
    app->swap_chain = NULL;
}

static bool modifier_key(UINT virtual_key)
{
    return virtual_key == VK_SHIFT || virtual_key == VK_CONTROL ||
           virtual_key == VK_MENU || virtual_key == VK_LWIN ||
           virtual_key == VK_RWIN;
}

static uint32_t pressed_modifiers(void)
{
    uint32_t modifiers = 0;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) modifiers |= CF_HOTKEY_CONTROL;
    if ((GetKeyState(VK_MENU) & 0x8000) != 0) modifiers |= CF_HOTKEY_ALT;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) modifiers |= CF_HOTKEY_SHIFT;
    if ((GetKeyState(VK_LWIN) & 0x8000) != 0 ||
        (GetKeyState(VK_RWIN) & 0x8000) != 0) modifiers |= CF_HOTKEY_WIN;
    return modifiers;
}

static void capture_hotkey(CfApp *app, UINT virtual_key)
{
    CfHotkey candidate = {pressed_modifiers(), virtual_key};
    CfHotkey old = {0, 0};
    bool old_has_macro = app->model.macro.has_hotkey;
    CfHotkeyCapture capture = app->model.capture;
    CfResult result = cf_hotkey_validate(&candidate);

    if (capture == CF_CAPTURE_NONE || modifier_key(virtual_key)) return;
    if (result == CF_OK) {
        if (capture == CF_CAPTURE_PRIMARY) {
            old = app->model.config.primary_hotkey;
            app->model.config.primary_hotkey = candidate;
        } else if (capture == CF_CAPTURE_EMERGENCY) {
            old = app->model.config.emergency_hotkey;
            app->model.config.emergency_hotkey = candidate;
        } else {
            old = app->model.macro.hotkey;
            app->model.macro.hotkey = candidate;
            app->model.macro.has_hotkey = true;
        }
        result = cf_hotkey_validate_pair(app->model.config.primary_hotkey,
                                         app->model.config.emergency_hotkey);
        if (result == CF_OK) result = register_hotkeys(app);
    }
    if (result != CF_OK) {
        if (capture == CF_CAPTURE_PRIMARY) {
            app->model.config.primary_hotkey = old;
        } else if (capture == CF_CAPTURE_EMERGENCY) {
            app->model.config.emergency_hotkey = old;
        } else {
            app->model.macro.hotkey = old;
            app->model.macro.has_hotkey = old_has_macro;
        }
        register_hotkeys(app);
        cf_app_show_toast(&app->model, cf_tr(app->model.config.language,
                          "该热键无效、重复或已被占用",
                          "That hotkey is invalid, duplicated, or unavailable"));
    } else {
        if (capture != CF_CAPTURE_MACRO) app->model.config_dirty = true;
        cf_app_show_toast(&app->model,
                          capture == CF_CAPTURE_MACRO
                              ? cf_tr(app->model.config.language,
                                      "宏热键已设置，保存宏后会写入文件",
                                      "Macro hotkey set. Save the macro to write it to file")
                              : cf_tr(app->model.config.language,
                                      "全局热键已更新", "Global hotkey updated"));
    }
    app->model.capture = CF_CAPTURE_NONE;
}

static bool task_active(CfTaskState state)
{
    return state == CF_TASK_STARTING || state == CF_TASK_RUNNING ||
           state == CF_TASK_PAUSED || state == CF_TASK_STOPPING;
}

static void handle_hotkey(CfApp *app, int identifier)
{
    CfTaskState state = cf_controller_state(app->model.controller);

    if (identifier == CF_HOTKEY_EMERGENCY_ID) {
        cf_controller_stop(app->model.controller);
        app->model.activity = CF_ACTIVITY_NONE;
        cf_app_show_toast(&app->model, cf_tr(app->model.config.language,
                          "任务已紧急停止", "Task stopped immediately"));
        return;
    }
    if (identifier == CF_HOTKEY_PRIMARY_ID) {
        if (task_active(state)) {
            cf_controller_stop(app->model.controller);
            cf_app_show_toast(&app->model, cf_tr(app->model.config.language,
                              "正在停止当前任务", "Stopping current task"));
        } else if (cf_controller_start_clicker(app->model.controller,
                                               &app->model.config.clicker) == CF_OK) {
            app->model.activity = CF_ACTIVITY_CLICKER;
            cf_app_show_toast(&app->model, cf_tr(app->model.config.language,
                              "连点已开始", "Clicker started"));
        } else {
            cf_app_show_toast(&app->model, cf_tr(app->model.config.language,
                              "连点参数无效", "Invalid clicker settings"));
        }
        return;
    }
    if (identifier == CF_HOTKEY_MACRO_ID && !task_active(state) &&
        app->model.macro_ready && app->model.macro.has_hotkey) {
        if (cf_controller_start_macro(app->model.controller, &app->model.macro,
                                      &app->model.macro_options) == CF_OK) {
            app->model.activity = CF_ACTIVITY_MACRO;
            cf_app_show_toast(&app->model, cf_tr(app->model.config.language,
                              "宏已由全局热键启动", "Macro started by global hotkey"));
        } else {
            cf_app_show_toast(&app->model, cf_tr(app->model.config.language,
                              "宏无效或屏幕布局已变化",
                              "Macro is invalid or the display layout changed"));
        }
    }
}

static LRESULT CALLBACK window_proc(HWND window, UINT message,
                                    WPARAM wparam, LPARAM lparam)
{
    CfApp *app = current_app;

    switch (message) {
    case WM_CLOSE:
        if (app != NULL && app->model.recording_dirty &&
            MessageBoxW(window, cf_trw(app->model.config.language,
                        L"当前录制尚未保存，确定退出？",
                        L"The current recording has not been saved. Exit anyway?"),
                        L"ClickFlow",
                        MB_ICONWARNING | MB_OKCANCEL) != IDOK) {
            return 0;
        }
        DestroyWindow(window);
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (app != NULL && app->model.capture != CF_CAPTURE_NONE &&
            (lparam & (1L << 30)) == 0) {
            capture_hotkey(app, (UINT)wparam);
            return 0;
        }
        break;
    case WM_HOTKEY:
        if (app != NULL && app->model.controller != NULL) {
            handle_hotkey(app, (int)wparam);
            InvalidateRect(window, NULL, FALSE);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        limits->ptMinTrackSize.x = CF_MIN_WIDTH;
        limits->ptMinTrackSize.y = CF_MIN_HEIGHT;
        return 0;
    }
    case WM_DPICHANGED: {
        RECT *suggested = (RECT *)lparam;
        SetWindowPos(window, NULL, suggested->left, suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOACTIVATE | SWP_NOZORDER);
        return 0;
    }
    case WM_SIZE:
        if (app != NULL && app->swap_chain != NULL && wparam != SIZE_MINIMIZED) {
            app->width = LOWORD(lparam);
            app->height = HIWORD(lparam);
            if (app->width > 0 && app->height > 0 &&
                create_render_target(app, app->width, app->height)) {
                nk_d3d11_resize(app->device_context, app->width, app->height);
            }
        }
        break;
    case CF_CONTROLLER_MESSAGE:
        if (app != NULL) {
            app->model.activity = CF_ACTIVITY_NONE;
            if ((CfResult)wparam == CF_OK ||
                (CfResult)wparam == CF_ERR_CANCELLED) {
                cf_app_show_toast(&app->model, cf_tr(app->model.config.language,
                                  "任务已结束", "Task finished"));
            } else {
                cf_app_show_toast(&app->model, cf_tr(app->model.config.language,
                                  "任务因执行错误而停止",
                                  "Task stopped because of an execution error"));
            }
            InvalidateRect(app->window, NULL, FALSE);
        }
        return 0;
    default:
        break;
    }

    if (app != NULL && app->ui != NULL &&
        nk_d3d11_handle_event(window, message, wparam, lparam)) {
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static bool create_window(CfApp *app, int show_command)
{
    WNDCLASSEXW window_class;
    RECT bounds = {0, 0, CF_INITIAL_WIDTH, CF_INITIAL_HEIGHT};
    DWORD style = WS_OVERLAPPEDWINDOW;

    memset(&window_class, 0, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_DBLCLKS | CS_OWNDC;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = app->instance;
    window_class.hIcon = LoadIconW(app->instance, MAKEINTRESOURCEW(101));
    window_class.hIconSm = (HICON)LoadImageW(app->instance, MAKEINTRESOURCEW(101),
        IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hbrBackground = NULL;
    window_class.lpszClassName = CF_WINDOW_CLASS;
    if (RegisterClassExW(&window_class) == 0) return false;

    AdjustWindowRectEx(&bounds, style, FALSE, WS_EX_APPWINDOW);
    app->window = CreateWindowExW(
        WS_EX_APPWINDOW, CF_WINDOW_CLASS, CF_WINDOW_TITLE, style,
        CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left,
        bounds.bottom - bounds.top, NULL, NULL, app->instance, NULL);
    if (app->window == NULL) return false;
    ShowWindow(app->window, show_command);
    UpdateWindow(app->window);
    return true;
}

static const nk_rune *clickflow_glyph_ranges(void)
{
    static const wchar_t glyphs[] =
        L"…○●、。一上下不与专且个中临为主他以件任份会但位作你保停光入全其内写击列创删制前加务动化单占即原参双取变另可右合同名后启命和回因固在地均坐增备复外多大失奏始字存完宏定容将小尚就局屏属左已布幕并序应度延建开式当录待得忙快态急恢成或所手打执抬拒择持指按捕换控操放效数文新方无时晰暂更替最有未本束条查标样格检次止正毫没法流浅消深添清滚点热焦状用由留的目知确离秒称移程空立等紧线组结绝继绪续编置而能自色节获行表被观视认记设该误请读败起超跟轮载辑输达过运还这连迟选速配重量错键长间限除随隔项顺题默鼠语言简体英语（），：；？";
    static nk_rune ranges[1024];
    static bool initialized;
    size_t index;
    size_t output = 0;

    if (initialized) return ranges;
    ranges[output++] = 0x0020;
    ranges[output++] = 0x00FF;
    for (index = 0; glyphs[index] != L'\0' && output + 2U < CF_ARRAY_COUNT(ranges);
         ++index) {
        ranges[output++] = (nk_rune)glyphs[index];
        ranges[output++] = (nk_rune)glyphs[index];
    }
    ranges[output] = 0;
    initialized = true;
    return ranges;
}

static struct nk_font *load_font(struct nk_font_atlas *atlas,
                                 float size, bool bold)
{
    wchar_t windows_path[MAX_PATH];
    wchar_t font_path[MAX_PATH];
    char utf8_path[MAX_PATH * 3];
    struct nk_font_config configuration = nk_font_config(0);
    const wchar_t *regular[] = {L"msyh.ttc", L"segoeui.ttf"};
    const wchar_t *strong[] = {L"msyhbd.ttc", L"msyh.ttc",
                               L"segoeuib.ttf", L"segoeui.ttf"};
    const wchar_t *const *candidates = bold ? strong : regular;
    size_t candidate_count = bold ? CF_ARRAY_COUNT(strong)
                                  : CF_ARRAY_COUNT(regular);
    size_t index;

    if (GetWindowsDirectoryW(windows_path, CF_ARRAY_COUNT(windows_path)) == 0) {
        return NULL;
    }
    configuration.range = clickflow_glyph_ranges();
    configuration.oversample_h = 3;
    configuration.oversample_v = 2;
    for (index = 0; index < candidate_count; ++index) {
        if (swprintf(font_path, CF_ARRAY_COUNT(font_path), L"%ls\\Fonts\\%ls",
                     windows_path, candidates[index]) < 0 ||
            GetFileAttributesW(font_path) == INVALID_FILE_ATTRIBUTES) {
            continue;
        }
        if (WideCharToMultiByte(CP_UTF8, 0, font_path, -1, utf8_path,
                                (int)sizeof(utf8_path), NULL, NULL) == 0) {
            continue;
        }
        return nk_font_atlas_add_from_file(atlas, utf8_path, size,
                                           &configuration);
    }
    return NULL;
}

static bool initialize_ui(CfApp *app)
{
    struct nk_font_atlas *atlas;
    struct nk_font *body_font;
    struct nk_font *heading_font;

    app->ui = nk_d3d11_init(app->device, app->width, app->height,
                            CF_MAX_VERTEX_BUFFER, CF_MAX_INDEX_BUFFER);
    if (app->ui == NULL) return false;
    nk_d3d11_font_stash_begin(&atlas);
    body_font = load_font(atlas, 22.0f, false);
    heading_font = load_font(atlas, 28.0f, true);
    nk_d3d11_font_stash_end();
    if (body_font != NULL) nk_style_set_font(app->ui, &body_font->handle);
    cf_ui_set_heading_font(heading_font != NULL ? &heading_font->handle
                                                : body_font != NULL
                                                      ? &body_font->handle
                                                      : NULL);
    cf_ui_apply_theme(app->ui, app->model.config.theme);
    return true;
}

static const char *page_title(CfPage page, CfLanguage language)
{
    static const char *zh[] = {"鼠标连点", "鼠标录制", "可视化宏", "设置"};
    static const char *en[] = {"Auto Clicker", "Mouse Recorder", "Visual Macro", "Settings"};
    return (language == CF_LANGUAGE_EN_US ? en : zh)[page];
}

static const char *page_description(CfPage page, CfLanguage language)
{
    static const char *zh[] = {
        "快速、可控地重复点击当前或固定位置",
        "记录鼠标动作并按原始节奏回放",
        "用清晰的动作列表组合自动化流程",
        "主题、默认参数与全局热键"
    };
    static const char *en[] = {
        "Repeat clicks at the cursor or a fixed position",
        "Capture mouse input and replay it at the original pace",
        "Build automation from a clear, editable action list",
        "Language, appearance, defaults, and global hotkeys"
    };
    return (language == CF_LANGUAGE_EN_US ? en : zh)[page];
}

static void draw_shell(CfApp *app)
{
    struct nk_context *context = app->ui;
    CfTheme theme = app->model.config.theme;
    CfLanguage language = app->model.config.language;
    CfTaskState state = cf_controller_state(app->model.controller);
    bool active = task_active(state);
    const char *status = state == CF_TASK_PAUSED ? cf_tr(language, "已暂停", "Paused") :
                         state == CF_TASK_STOPPING ? cf_tr(language, "停止中", "Stopping") :
                         active ? cf_tr(language, "运行中", "Running")
                                : cf_tr(language, "就绪", "Ready");
    char emergency[64] = "Esc";
    char activity_hint[128];

    cf_hotkey_format_utf8(app->model.config.emergency_hotkey,
                          emergency, sizeof(emergency));
    snprintf(activity_hint, sizeof(activity_hint),
             active ? cf_tr(language, "按 %s 可立即停止", "Press %s to stop immediately")
                    : cf_tr(language, "所有任务均由你主动启动", "Tasks only run when you start them"),
             emergency);

    if (!nk_begin(context, "ClickFlowRoot",
                  nk_rect(0, 0, (float)app->width, (float)app->height),
                  NK_WINDOW_NO_SCROLLBAR)) {
        nk_end(context);
        return;
    }

    nk_layout_row_begin(context, NK_STATIC, (float)app->height - 44.0f, 2);
    nk_layout_row_push(context, (float)CF_NAV_WIDTH);
    if (cf_ui_panel_begin(context, "Navigation", NK_WINDOW_NO_SCROLLBAR,
                          false, theme)) {
        nk_layout_row_dynamic(context, 42.0f, 1);
        nk_label(context, "CLICKFLOW", NK_TEXT_CENTERED);
        nk_layout_row_dynamic(context, 26.0f, 1);
        nk_label(context, cf_tr(language, "本地自动化", "AUTOMATION"), NK_TEXT_CENTERED);
        nk_layout_row_dynamic(context, 24.0f, 1);
        nk_spacing(context, 1);
        nk_layout_row_dynamic(context, 52.0f, 1);
        if (cf_ui_nav_button(context, cf_tr(language, "连点", "Clicker"), app->model.page == CF_PAGE_CLICKER, theme)) app->model.page = CF_PAGE_CLICKER;
        nk_layout_row_dynamic(context, 52.0f, 1);
        if (cf_ui_nav_button(context, cf_tr(language, "录制", "Recorder"), app->model.page == CF_PAGE_RECORDING, theme)) app->model.page = CF_PAGE_RECORDING;
        nk_layout_row_dynamic(context, 52.0f, 1);
        if (cf_ui_nav_button(context, cf_tr(language, "宏", "Macros"), app->model.page == CF_PAGE_MACRO, theme)) app->model.page = CF_PAGE_MACRO;
        nk_layout_row_dynamic(context, 52.0f, 1);
        if (cf_ui_nav_button(context, cf_tr(language, "设置", "Settings"), app->model.page == CF_PAGE_SETTINGS, theme)) app->model.page = CF_PAGE_SETTINGS;
        nk_layout_row_dynamic(context, (float)app->height - 454.0f, 1);
        nk_spacing(context, 1);
        nk_layout_row_dynamic(context, 26.0f, 1);
        nk_label(context, cf_tr(language, "本地 · 离线", "LOCAL · OFFLINE"), NK_TEXT_CENTERED);
        nk_group_end(context);
    }
    nk_layout_row_push(context, (float)app->width -
                                (float)(CF_NAV_WIDTH + CF_SHELL_GUTTER));
    if (cf_ui_panel_begin(context, "Workspace", 0, false, theme)) {
        cf_ui_page_heading(context, page_title(app->model.page, language),
                           page_description(app->model.page, language), theme);
        nk_layout_row_begin(context, NK_STATIC, 36.0f, 2);
        nk_layout_row_push(context, 128.0f);
        cf_ui_status_badge(context, status, active, theme);
        nk_layout_row_push(context, 320.0f);
        nk_label(context, activity_hint, NK_TEXT_LEFT);
        nk_layout_row_end(context);

        if (app->model.toast[0] != '\0' &&
            GetTickCount64() < app->model.toast_until_ms) {
            nk_layout_row_dynamic(context, 28.0f, 1);
            cf_ui_status_badge(context, app->model.toast, false, theme);
        } else {
            app->model.toast[0] = '\0';
            nk_layout_row_dynamic(context, 8.0f, 1);
            nk_spacing(context, 1);
        }

        switch (app->model.page) {
        case CF_PAGE_CLICKER:
            cf_ui_draw_clicker_page(context, &app->model);
            break;
        case CF_PAGE_RECORDING:
            cf_ui_draw_recording_page(context, &app->model);
            break;
        case CF_PAGE_MACRO:
            cf_ui_draw_macro_page(context, &app->model);
            break;
        case CF_PAGE_SETTINGS:
            cf_ui_draw_settings_page(context, &app->model);
            break;
        }
        nk_group_end(context);
    }
    nk_layout_row_end(context);
    nk_end(context);
}

static void render(CfApp *app)
{
    CfUiPalette palette = cf_ui_palette(app->model.config.theme);
    float clear[4] = {
        palette.background.r / 255.0f,
        palette.background.g / 255.0f,
        palette.background.b / 255.0f,
        1.0f
    };

    draw_shell(app);
    ID3D11DeviceContext_ClearRenderTargetView(app->device_context,
                                              app->render_target, clear);
    ID3D11DeviceContext_OMSetRenderTargets(app->device_context, 1,
                                           &app->render_target, NULL);
    nk_d3d11_render(app->device_context, NK_ANTI_ALIASING_ON);
    if (IDXGISwapChain_Present(app->swap_chain, 1, 0) == DXGI_STATUS_OCCLUDED) {
        Sleep(10);
    }
}

static bool initialize_app(CfApp *app, HINSTANCE instance, int show_command)
{
    BOOL animations = TRUE;

    memset(app, 0, sizeof(*app));
    app->instance = instance;
    app->width = CF_INITIAL_WIDTH;
    app->height = CF_INITIAL_HEIGHT;
    app->running = true;
    app->model.page = CF_PAGE_CLICKER;
    app->model.selected_action = -1;
    app->model.recording_options = (CfRunOptions){false, 1, 1.0};
    app->model.macro_options = (CfRunOptions){false, 1, 1.0};
    cf_macro_init(&app->model.recording);
    cf_macro_init(&app->model.macro);
    app->model.macro.recorded_screen = cf_app_virtual_screen();
    app->model.macro_ready = true;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
    app->model.animations_enabled = animations;

    if (cf_single_instance_acquire(&app->single_instance) == CF_ERR_CONFLICT) {
        cf_single_instance_activate_existing(CF_WINDOW_CLASS);
        return false;
    }
    if (cf_win32_ensure_data_directories() != CF_OK) return false;
    load_config(&app->model);
    cf_macro_set_name(&app->model.macro,
                      cf_tr(app->model.config.language, "新建宏", "New macro"));
    snprintf(app->model.macro_name, sizeof(app->model.macro_name), "%s",
             cf_tr(app->model.config.language, "新建宏", "New macro"));
    snprintf(app->model.recording_name, sizeof(app->model.recording_name), "%s",
             cf_tr(app->model.config.language, "未命名录制", "Untitled recording"));
    if (!create_window(app, show_command) || !create_d3d(app)) return false;
    app->model.window = app->window;
    if (!initialize_ui(app)) return false;

    app->model.controller = cf_controller_create(app->window,
                                                  CF_CONTROLLER_MESSAGE);
    if (app->model.controller == NULL ||
        cf_win32_input_init(&app->input,
                            cf_controller_stop_event(app->model.controller),
                            cf_controller_pause_event(app->model.controller),
                            cf_controller_resume_event(app->model.controller)) != CF_OK ||
        cf_controller_set_execution_ops(app->model.controller,
                                        cf_win32_input_ops(&app->input)) != CF_OK) {
        return false;
    }
    if (register_hotkeys(app) != CF_OK) {
        cf_config_defaults(&app->model.config);
        if (register_hotkeys(app) != CF_OK) {
            cf_app_show_toast(&app->model, cf_tr(app->model.config.language,
                              "全局热键被占用，请在设置中重新指定",
                              "Global hotkeys are unavailable. Choose new ones in Settings"));
        } else {
            app->model.config_dirty = true;
            cf_app_show_toast(&app->model, cf_tr(app->model.config.language,
                              "已恢复默认热键，原热键当前不可用",
                              "Default hotkeys restored because the saved ones are unavailable"));
        }
    }
    return true;
}

static void cleanup_app(CfApp *app)
{
    wchar_t path[MAX_PATH];
    char error[160];

    if (app->model.config_dirty && config_path(path, CF_ARRAY_COUNT(path))) {
        cf_json_save_config_atomic(path, &app->model.config,
                                   error, sizeof(error));
    }
    unregister_hotkeys(app);
    if (app->model.controller != NULL) cf_controller_destroy(app->model.controller);
    cf_win32_input_shutdown(&app->input);
    cf_macro_free(&app->model.recording);
    cf_macro_free(&app->model.macro);
    if (app->ui != NULL) nk_d3d11_shutdown();
    destroy_d3d(app);
    if (app->window != NULL) DestroyWindow(app->window);
    UnregisterClassW(CF_WINDOW_CLASS, app->instance);
    cf_single_instance_release(&app->single_instance);
}

int cf_app_run(HINSTANCE instance, int show_command)
{
    CfApp app;
    MSG message;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    current_app = &app;
    if (!initialize_app(&app, instance, show_command)) {
        cleanup_app(&app);
        current_app = NULL;
        CoUninitialize();
        return 1;
    }

    while (app.running) {
        nk_input_begin(app.ui);
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) app.running = false;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        nk_input_end(app.ui);
        if (app.running && !IsIconic(app.window)) render(&app);
        else Sleep(10);
    }

    cleanup_app(&app);
    current_app = NULL;
    CoUninitialize();
    return 0;
}
