#ifndef CLICKFLOW_UI_THEME_H
#define CLICKFLOW_UI_THEME_H

#include "core/config.h"

struct nk_color;
struct nk_context;

typedef struct CfUiPalette {
    struct nk_color background;
    struct nk_color card;
    struct nk_color raised;
    struct nk_color text;
    struct nk_color muted;
    struct nk_color border;
    struct nk_color accent;
    struct nk_color accent_hover;
    struct nk_color danger;
} CfUiPalette;

CfUiPalette cf_ui_palette(CfTheme theme);
void cf_ui_apply_theme(struct nk_context *context, CfTheme theme);

#endif
