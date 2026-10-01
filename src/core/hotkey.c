#include "core/hotkey.h"

#include <stdio.h>
#include <string.h>

enum {
    CF_VK_BACK = 0x08,
    CF_VK_TAB = 0x09,
    CF_VK_RETURN = 0x0D,
    CF_VK_SHIFT = 0x10,
    CF_VK_CONTROL = 0x11,
    CF_VK_MENU = 0x12,
    CF_VK_ESCAPE = 0x1B,
    CF_VK_SPACE = 0x20,
    CF_VK_LEFT = 0x25,
    CF_VK_UP = 0x26,
    CF_VK_RIGHT = 0x27,
    CF_VK_DOWN = 0x28,
    CF_VK_LWIN = 0x5B,
    CF_VK_RWIN = 0x5C,
    CF_VK_F1 = 0x70,
    CF_VK_F24 = 0x87
};

static bool modifier_key(uint32_t virtual_key)
{
    return virtual_key == CF_VK_SHIFT || virtual_key == CF_VK_CONTROL ||
           virtual_key == CF_VK_MENU || virtual_key == CF_VK_LWIN ||
           virtual_key == CF_VK_RWIN;
}

CfResult cf_hotkey_validate(const CfHotkey *hotkey)
{
    const uint32_t allowed = CF_HOTKEY_ALT | CF_HOTKEY_CONTROL |
                             CF_HOTKEY_SHIFT | CF_HOTKEY_WIN;

    if (hotkey == NULL || hotkey->virtual_key == 0 ||
        hotkey->virtual_key > 0xFEU || (hotkey->modifiers & ~allowed) != 0 ||
        modifier_key(hotkey->virtual_key)) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    return CF_OK;
}

CfResult cf_hotkey_validate_pair(CfHotkey primary, CfHotkey emergency)
{
    CfResult result = cf_hotkey_validate(&primary);

    if (result != CF_OK) {
        return result;
    }
    result = cf_hotkey_validate(&emergency);
    if (result != CF_OK) {
        return result;
    }
    return cf_hotkey_equal(primary, emergency) ? CF_ERR_CONFLICT : CF_OK;
}

bool cf_hotkey_equal(CfHotkey left, CfHotkey right)
{
    return left.modifiers == right.modifiers &&
           left.virtual_key == right.virtual_key;
}

static const char *named_key(uint32_t virtual_key)
{
    switch (virtual_key) {
    case CF_VK_BACK:
        return "Backspace";
    case CF_VK_TAB:
        return "Tab";
    case CF_VK_RETURN:
        return "Enter";
    case CF_VK_ESCAPE:
        return "Esc";
    case CF_VK_SPACE:
        return "Space";
    case CF_VK_LEFT:
        return "Left";
    case CF_VK_UP:
        return "Up";
    case CF_VK_RIGHT:
        return "Right";
    case CF_VK_DOWN:
        return "Down";
    default:
        return NULL;
    }
}

CfResult cf_hotkey_format_utf8(CfHotkey hotkey, char *buffer, size_t size)
{
    char formatted[64] = {0};
    char key_name[16];
    const char *key;
    int written;
    size_t used = 0;

    if (buffer == NULL || size == 0 || cf_hotkey_validate(&hotkey) != CF_OK) {
        return CF_ERR_INVALID_ARGUMENT;
    }

#define CF_APPEND_MODIFIER(flag, text)                                           \
    do {                                                                         \
        if ((hotkey.modifiers & (flag)) != 0) {                                   \
            written = snprintf(formatted + used, sizeof(formatted) - used,        \
                               "%s+", (text));                                  \
            if (written < 0 || (size_t)written >= sizeof(formatted) - used) {     \
                return CF_ERR_LIMIT;                                              \
            }                                                                    \
            used += (size_t)written;                                              \
        }                                                                        \
    } while (0)

    CF_APPEND_MODIFIER(CF_HOTKEY_CONTROL, "Ctrl");
    CF_APPEND_MODIFIER(CF_HOTKEY_ALT, "Alt");
    CF_APPEND_MODIFIER(CF_HOTKEY_SHIFT, "Shift");
    CF_APPEND_MODIFIER(CF_HOTKEY_WIN, "Win");

#undef CF_APPEND_MODIFIER

    key = named_key(hotkey.virtual_key);
    if (key == NULL && hotkey.virtual_key >= CF_VK_F1 &&
        hotkey.virtual_key <= CF_VK_F24) {
        snprintf(key_name, sizeof(key_name), "F%u",
                 (unsigned)(hotkey.virtual_key - CF_VK_F1 + 1U));
        key = key_name;
    } else if (key == NULL &&
               ((hotkey.virtual_key >= '0' && hotkey.virtual_key <= '9') ||
                (hotkey.virtual_key >= 'A' && hotkey.virtual_key <= 'Z'))) {
        key_name[0] = (char)hotkey.virtual_key;
        key_name[1] = '\0';
        key = key_name;
    } else if (key == NULL) {
        snprintf(key_name, sizeof(key_name), "VK_%02X",
                 (unsigned)hotkey.virtual_key);
        key = key_name;
    }

    written = snprintf(formatted + used, sizeof(formatted) - used, "%s", key);
    if (written < 0 || (size_t)written >= sizeof(formatted) - used) {
        return CF_ERR_LIMIT;
    }
    used += (size_t)written;
    if (used + 1U > size) {
        return CF_ERR_LIMIT;
    }
    memcpy(buffer, formatted, used + 1U);
    return CF_OK;
}
