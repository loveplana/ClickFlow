#include "nuklear.h"

#include "ui/theme.h"

static struct nk_color rgb(unsigned value)
{
    return nk_rgb((nk_byte)((value >> 16U) & 0xFFU),
                  (nk_byte)((value >> 8U) & 0xFFU),
                  (nk_byte)(value & 0xFFU));
}

CfUiPalette cf_ui_palette(CfTheme theme)
{
    CfUiPalette palette;

    if (theme == CF_THEME_LIGHT) {
        palette.background = rgb(0xF4F7FA);
        palette.card = rgb(0xFFFFFF);
        palette.raised = rgb(0xEAF0F5);
        palette.text = rgb(0x121923);
        palette.muted = rgb(0x5F6D7C);
        palette.border = rgb(0xD5DEE7);
    } else {
        palette.background = rgb(0x0B0E13);
        palette.card = rgb(0x131821);
        palette.raised = rgb(0x1D2530);
        palette.text = rgb(0xF7F9FC);
        palette.muted = rgb(0xA7B1BE);
        palette.border = rgb(0x2C3745);
    }
    palette.accent = rgb(0x35D49A);
    palette.accent_hover = rgb(0x4BE2AA);
    palette.danger = rgb(0xF0646E);
    return palette;
}

void cf_ui_apply_theme(struct nk_context *context, CfTheme theme)
{
    CfUiPalette palette = cf_ui_palette(theme);
    struct nk_color table[NK_COLOR_COUNT];

    table[NK_COLOR_TEXT] = palette.text;
    table[NK_COLOR_WINDOW] = palette.background;
    table[NK_COLOR_HEADER] = palette.card;
    table[NK_COLOR_BORDER] = palette.border;
    table[NK_COLOR_BUTTON] = palette.raised;
    table[NK_COLOR_BUTTON_HOVER] = palette.raised;
    table[NK_COLOR_BUTTON_ACTIVE] = palette.accent;
    table[NK_COLOR_TOGGLE] = palette.raised;
    table[NK_COLOR_TOGGLE_HOVER] = palette.border;
    table[NK_COLOR_TOGGLE_CURSOR] = palette.accent;
    table[NK_COLOR_SELECT] = palette.raised;
    table[NK_COLOR_SELECT_ACTIVE] = palette.accent;
    table[NK_COLOR_SLIDER] = palette.raised;
    table[NK_COLOR_SLIDER_CURSOR] = palette.accent;
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = palette.accent_hover;
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = palette.accent;
    table[NK_COLOR_PROPERTY] = palette.card;
    table[NK_COLOR_EDIT] = palette.raised;
    table[NK_COLOR_EDIT_CURSOR] = palette.text;
    table[NK_COLOR_COMBO] = palette.raised;
    table[NK_COLOR_CHART] = palette.card;
    table[NK_COLOR_CHART_COLOR] = palette.accent;
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = palette.accent_hover;
    table[NK_COLOR_SCROLLBAR] = palette.card;
    table[NK_COLOR_SCROLLBAR_CURSOR] = palette.border;
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = palette.muted;
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = palette.accent;
    table[NK_COLOR_TAB_HEADER] = palette.card;
    nk_style_from_table(context, table);

    context->style.window.background = palette.background;
    context->style.window.fixed_background = nk_style_item_color(palette.background);
    context->style.window.padding = nk_vec2(22.0f, 22.0f);
    context->style.window.group_padding = nk_vec2(20.0f, 18.0f);
    context->style.window.spacing = nk_vec2(12.0f, 12.0f);
    context->style.window.border = 0.0f;
    context->style.window.group_border = 1.0f;
    context->style.window.group_border_color = palette.border;
    context->style.button.rounding = 10.0f;
    context->style.button.border = 1.0f;
    context->style.button.border_color = palette.border;
    context->style.button.padding = nk_vec2(14.0f, 9.0f);
    context->style.option.padding = nk_vec2(8.0f, 6.0f);
    context->style.option.spacing = 8.0f;
    context->style.edit.rounding = 9.0f;
    context->style.edit.border = 1.0f;
    context->style.edit.border_color = palette.border;
    context->style.property.rounding = 9.0f;
    context->style.property.border = 1.0f;
    context->style.property.border_color = palette.border;
    context->style.combo.rounding = 9.0f;
    context->style.combo.border = 1.0f;
    context->style.combo.border_color = palette.border;
}
