#ifndef CLICKFLOW_UI_I18N_H
#define CLICKFLOW_UI_I18N_H

#include "core/config.h"

#include <wchar.h>

static inline const char *cf_tr(CfLanguage language,
                                const char *chinese,
                                const char *english)
{
    return language == CF_LANGUAGE_EN_US ? english : chinese;
}

static inline const wchar_t *cf_trw(CfLanguage language,
                                    const wchar_t *chinese,
                                    const wchar_t *english)
{
    return language == CF_LANGUAGE_EN_US ? english : chinese;
}

#endif
