#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#define CF_COUNTER_CLASS L"ClickFlowClickCounter"
#define CF_COUNTER_TITLE L"ClickFlow Click Counter"
#define CF_RESET_BUTTON 1001

typedef struct CounterState {
    uint64_t total;
    uint64_t left;
    uint64_t middle;
    uint64_t right;
    HFONT title_font;
    HFONT count_font;
    HFONT body_font;
    HWND reset_button;
    BOOL english;
} CounterState;

static HFONT create_ui_font(HWND window, int points, int weight)
{
    HDC device = GetDC(window);
    int dpi = device != NULL ? GetDeviceCaps(device, LOGPIXELSY) : 96;
    if (device != NULL) ReleaseDC(window, device);
    return CreateFontW(-MulDiv(points, dpi, 72), 0, 0, 0, weight, FALSE,
                       FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
}

static void layout_controls(HWND window, CounterState *state)
{
    RECT client;
    int width;

    GetClientRect(window, &client);
    width = client.right - client.left;
    MoveWindow(state->reset_button, width / 2 - 72, client.bottom - 62,
               144, 38, TRUE);
}

static void reset_counts(CounterState *state)
{
    state->total = 0;
    state->left = 0;
    state->middle = 0;
    state->right = 0;
}

static void update_title(HWND window, const CounterState *state)
{
    wchar_t title[128];
    swprintf(title, sizeof(title) / sizeof(title[0]), state->english
                 ? L"ClickFlow Click Counter — Total: %llu"
                 : L"ClickFlow 点击计数测试器 — 总数：%llu",
             (unsigned long long)state->total);
    SetWindowTextW(window, title);
}

static void count_click(HWND window, CounterState *state, UINT message)
{
    ++state->total;
    if (message == WM_LBUTTONDOWN) ++state->left;
    else if (message == WM_MBUTTONDOWN) ++state->middle;
    else if (message == WM_RBUTTONDOWN) ++state->right;
    update_title(window, state);
    InvalidateRect(window, NULL, FALSE);
}

static void draw_centered(HDC device, RECT bounds, const wchar_t *text,
                          HFONT font, COLORREF color, UINT format)
{
    HFONT old_font = SelectObject(device, font);
    SetTextColor(device, color);
    SetBkMode(device, TRANSPARENT);
    DrawTextW(device, text, -1, &bounds, DT_CENTER | DT_SINGLELINE |
              DT_VCENTER | format);
    SelectObject(device, old_font);
}

static void paint_window(HWND window, CounterState *state)
{
    PAINTSTRUCT paint;
    HDC device = BeginPaint(window, &paint);
    RECT client;
    RECT area;
    HBRUSH background = CreateSolidBrush(RGB(17, 19, 24));
    HBRUSH card = CreateSolidBrush(RGB(26, 29, 36));
    HPEN border = CreatePen(PS_SOLID, 2, RGB(40, 199, 139));
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    wchar_t text[160];

    GetClientRect(window, &client);
    FillRect(device, &client, background);

    area = (RECT){28, 24, client.right - 28, client.bottom - 82};
    old_pen = SelectObject(device, border);
    old_brush = SelectObject(device, card);
    RoundRect(device, area.left, area.top, area.right, area.bottom, 18, 18);
    SelectObject(device, old_brush);
    SelectObject(device, old_pen);

    area = (RECT){48, 38, client.right - 48, 82};
    draw_centered(device, area, state->english
                      ? L"Place the cursor here and start ClickFlow"
                      : L"把鼠标放在此区域并启动 ClickFlow",
                  state->title_font, RGB(244, 246, 248), 0);

    swprintf(text, sizeof(text) / sizeof(text[0]), L"%llu",
             (unsigned long long)state->total);
    area = (RECT){48, 82, client.right - 48, 186};
    draw_centered(device, area, text, state->count_font,
                  RGB(40, 199, 139), 0);

    area = (RECT){48, 178, client.right - 48, 214};
    draw_centered(device, area, state->english ? L"TOTAL CLICKS" : L"总点击数",
                  state->body_font, RGB(150, 158, 170), 0);

    swprintf(text, sizeof(text) / sizeof(text[0]), state->english
                 ? L"Left  %llu        Middle  %llu        Right  %llu"
                 : L"左键  %llu        中键  %llu        右键  %llu",
             (unsigned long long)state->left,
             (unsigned long long)state->middle,
             (unsigned long long)state->right);
    area = (RECT){48, 220, client.right - 48, 266};
    draw_centered(device, area, text, state->body_font,
                  RGB(244, 246, 248), 0);

    DeleteObject(border);
    DeleteObject(card);
    DeleteObject(background);
    EndPaint(window, &paint);
}

static LRESULT CALLBACK counter_proc(HWND window, UINT message,
                                     WPARAM wparam, LPARAM lparam)
{
    CounterState *state = (CounterState *)GetWindowLongPtrW(window,
                                                            GWLP_USERDATA);

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
        state = (CounterState *)create->lpCreateParams;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)state);
        state->english = PRIMARYLANGID(GetUserDefaultUILanguage()) != LANG_CHINESE;
        state->title_font = create_ui_font(window, 17, FW_SEMIBOLD);
        state->count_font = create_ui_font(window, 54, FW_BOLD);
        state->body_font = create_ui_font(window, 14, FW_NORMAL);
        state->reset_button = CreateWindowExW(
            0, L"BUTTON", state->english ? L"Reset" : L"重置计数",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
            window, (HMENU)(INT_PTR)CF_RESET_BUTTON,
            create->hInstance, NULL);
        SendMessageW(state->reset_button, WM_SETFONT,
                     (WPARAM)state->body_font, TRUE);
        layout_controls(window, state);
        update_title(window, state);
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (state != NULL) count_click(window, state, message);
        return 0;
    case WM_COMMAND:
        if (state != NULL && LOWORD(wparam) == CF_RESET_BUTTON) {
            reset_counts(state);
            update_title(window, state);
            InvalidateRect(window, NULL, FALSE);
        }
        return 0;
    case WM_KEYDOWN:
        if (state != NULL && (wparam == 'R' || wparam == VK_DELETE)) {
            reset_counts(state);
            update_title(window, state);
            InvalidateRect(window, NULL, FALSE);
        }
        return 0;
    case WM_SIZE:
        if (state != NULL && state->reset_button != NULL) {
            layout_controls(window, state);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        if (state != NULL) paint_window(window, state);
        return 0;
    case WM_DESTROY:
        if (state != NULL) {
            DeleteObject(state->title_font);
            DeleteObject(state->count_font);
            DeleteObject(state->body_font);
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous,
                   char *command_line, int show_command)
{
    WNDCLASSEXW window_class = {0};
    CounterState state = {0};
    RECT bounds = {0, 0, 620, 390};
    HWND window;
    MSG message = {0};

    (void)previous;
    (void)command_line;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = counter_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(NULL, IDC_CROSS);
    window_class.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    window_class.lpszClassName = CF_COUNTER_CLASS;
    if (RegisterClassExW(&window_class) == 0) return 1;

    AdjustWindowRectEx(&bounds, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW);
    window = CreateWindowExW(
        WS_EX_APPWINDOW, CF_COUNTER_CLASS, CF_COUNTER_TITLE,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        bounds.right - bounds.left, bounds.bottom - bounds.top,
        NULL, NULL, instance, &state);
    if (window == NULL) return 1;
    ShowWindow(window, show_command);
    UpdateWindow(window);

    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}
