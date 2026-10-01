#ifndef CLICKFLOW_HOTKEY_H
#define CLICKFLOW_HOTKEY_H

#include "core/cf_result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    CF_HOTKEY_ALT = 0x0001U,
    CF_HOTKEY_CONTROL = 0x0002U,
    CF_HOTKEY_SHIFT = 0x0004U,
    CF_HOTKEY_WIN = 0x0008U
};

typedef struct CfHotkey {
    uint32_t modifiers;
    uint32_t virtual_key;
} CfHotkey;

CfResult cf_hotkey_validate(const CfHotkey *hotkey);
CfResult cf_hotkey_validate_pair(CfHotkey primary, CfHotkey emergency);
bool cf_hotkey_equal(CfHotkey left, CfHotkey right);
CfResult cf_hotkey_format_utf8(CfHotkey hotkey, char *buffer, size_t size);

#endif
