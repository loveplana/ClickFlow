#ifndef CLICKFLOW_UI_WIDGETS_H
#define CLICKFLOW_UI_WIDGETS_H

#include "core/config.h"

#include <stdbool.h>
#include <stddef.h>
#include <windows.h>

struct nk_context;
struct nk_user_font;

void cf_ui_set_heading_font(const struct nk_user_font *font);
bool cf_ui_panel_begin(struct nk_context *context, const char *identifier,
                       unsigned int flags, bool raised, CfTheme theme);

bool cf_ui_nav_button(struct nk_context *context, const char *label,
                      bool active, CfTheme theme);
void cf_ui_page_heading(struct nk_context *context, const char *title,
                        const char *description, CfTheme theme);
void cf_ui_status_badge(struct nk_context *context, const char *label,
                        bool active, CfTheme theme);
bool cf_ui_primary_button(struct nk_context *context, const char *label,
                          bool danger, CfTheme theme);
void cf_ui_card_label(struct nk_context *context, const char *title,
                      const char *detail, CfTheme theme);
bool cf_ui_open_json_path(HWND owner, CfLanguage language,
                          wchar_t *path, size_t count);
bool cf_ui_save_json_path(HWND owner, CfLanguage language,
                          wchar_t *path, size_t count);

#endif
