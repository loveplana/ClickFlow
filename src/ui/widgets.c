#include "nuklear.h"

#include "ui/theme.h"
#include "ui/i18n.h"
#include "ui/widgets.h"

#include <commdlg.h>
#include <string.h>

static const struct nk_user_font *heading_font;

void cf_ui_set_heading_font(const struct nk_user_font *font)
{
    heading_font = font;
}

bool cf_ui_panel_begin(struct nk_context *context, const char *identifier,
                       unsigned int flags, bool raised, CfTheme theme)
{
    CfUiPalette palette = cf_ui_palette(theme);
    struct nk_color saved_background = context->style.window.background;
    struct nk_style_item saved_fixed = context->style.window.fixed_background;
    struct nk_color saved_border = context->style.window.group_border_color;
    bool opened;

    context->style.window.background = raised ? palette.raised : palette.card;
    context->style.window.fixed_background = nk_style_item_color(
        raised ? palette.raised : palette.card);
    context->style.window.group_border_color = palette.border;
    opened = nk_group_begin(context, identifier, flags | NK_WINDOW_BORDER) != 0;
    context->style.window.background = saved_background;
    context->style.window.fixed_background = saved_fixed;
    context->style.window.group_border_color = saved_border;
    return opened;
}

bool cf_ui_nav_button(struct nk_context *context, const char *label,
                      bool active, CfTheme theme)
{
    CfUiPalette palette = cf_ui_palette(theme);
    struct nk_style_button saved = context->style.button;
    bool clicked;

    context->style.button.normal = nk_style_item_color(active ? palette.raised : palette.card);
    context->style.button.hover = nk_style_item_color(palette.raised);
    context->style.button.active = nk_style_item_color(palette.accent);
    context->style.button.text_normal = active ? palette.accent : palette.muted;
    context->style.button.text_hover = palette.text;
    context->style.button.text_active = nk_rgb(17, 19, 24);
    context->style.button.border = active ? 1.0f : 0.0f;
    context->style.button.border_color = active ? palette.accent : palette.card;
    context->style.button.rounding = 11.0f;
    clicked = nk_button_label(context, label) != 0;
    context->style.button = saved;
    return clicked;
}

void cf_ui_page_heading(struct nk_context *context, const char *title,
                        const char *description, CfTheme theme)
{
    CfUiPalette palette = cf_ui_palette(theme);
    const struct nk_user_font *saved_font = context->style.font;
    context->style.text.color = palette.text;
    if (heading_font != NULL) context->style.font = heading_font;
    nk_layout_row_dynamic(context, 42.0f, 1);
    nk_label(context, title, NK_TEXT_LEFT);
    context->style.font = saved_font;
    context->style.text.color = palette.muted;
    nk_layout_row_dynamic(context, 28.0f, 1);
    nk_label(context, description, NK_TEXT_LEFT);
    context->style.text.color = palette.text;
}

void cf_ui_status_badge(struct nk_context *context, const char *label,
                        bool active, CfTheme theme)
{
    CfUiPalette palette = cf_ui_palette(theme);
    struct nk_style_button saved = context->style.button;
    context->style.button.normal = nk_style_item_color(active ? palette.accent : palette.raised);
    context->style.button.hover = context->style.button.normal;
    context->style.button.active = context->style.button.normal;
    context->style.button.text_normal = active ? nk_rgb(17, 19, 24) : palette.muted;
    context->style.button.border = 0.0f;
    context->style.button.rounding = 14.0f;
    nk_button_label(context, label);
    context->style.button = saved;
}

bool cf_ui_primary_button(struct nk_context *context, const char *label,
                          bool danger, CfTheme theme)
{
    CfUiPalette palette = cf_ui_palette(theme);
    struct nk_style_button saved = context->style.button;
    struct nk_color color = danger ? palette.danger : palette.accent;
    bool clicked;

    context->style.button.normal = nk_style_item_color(color);
    context->style.button.hover = nk_style_item_color(danger ? palette.danger : palette.accent_hover);
    context->style.button.active = nk_style_item_color(color);
    context->style.button.text_normal = nk_rgb(17, 19, 24);
    context->style.button.text_hover = nk_rgb(17, 19, 24);
    context->style.button.text_active = nk_rgb(17, 19, 24);
    context->style.button.border = 0.0f;
    context->style.button.rounding = 11.0f;
    clicked = nk_button_label(context, label) != 0;
    context->style.button = saved;
    return clicked;
}

void cf_ui_card_label(struct nk_context *context, const char *title,
                      const char *detail, CfTheme theme)
{
    CfUiPalette palette = cf_ui_palette(theme);
    context->style.text.color = palette.text;
    nk_layout_row_dynamic(context, 24.0f, 1);
    nk_label(context, title, NK_TEXT_LEFT);
    context->style.text.color = palette.muted;
    nk_layout_row_dynamic(context, 20.0f, 1);
    nk_label(context, detail, NK_TEXT_LEFT);
    context->style.text.color = palette.text;
}

static bool choose_json_path(HWND owner, CfLanguage language,
                             wchar_t *path, size_t count, bool save)
{
    OPENFILENAMEW dialog;

    if (path == NULL || count == 0 || count > UINT32_MAX) return false;
    memset(&dialog, 0, sizeof(dialog));
    if (!save) path[0] = L'\0';
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = cf_trw(
        language,
        L"ClickFlow JSON (*.json)\0*.json\0所有文件 (*.*)\0*.*\0\0",
        L"ClickFlow JSON (*.json)\0*.json\0All files (*.*)\0*.*\0\0");
    dialog.lpstrFile = path;
    dialog.nMaxFile = (DWORD)count;
    dialog.lpstrDefExt = L"json";
    dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
                   (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    return save ? GetSaveFileNameW(&dialog) != 0
                : GetOpenFileNameW(&dialog) != 0;
}

bool cf_ui_open_json_path(HWND owner, CfLanguage language,
                          wchar_t *path, size_t count)
{
    return choose_json_path(owner, language, path, count, false);
}

bool cf_ui_save_json_path(HWND owner, CfLanguage language,
                          wchar_t *path, size_t count)
{
    return choose_json_path(owner, language, path, count, true);
}
