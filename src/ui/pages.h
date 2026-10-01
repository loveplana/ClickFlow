#ifndef CLICKFLOW_UI_PAGES_H
#define CLICKFLOW_UI_PAGES_H

#include "ui/app.h"

struct nk_context;

void cf_ui_draw_clicker_page(struct nk_context *context, CfAppModel *model);
void cf_ui_draw_recording_page(struct nk_context *context, CfAppModel *model);
void cf_ui_draw_macro_page(struct nk_context *context, CfAppModel *model);
void cf_ui_draw_settings_page(struct nk_context *context, CfAppModel *model);

#endif
