#include "platform/win32_hotkey.h"

CfResult cf_win32_hotkey_register(HWND window, int identifier,
                                  CfHotkey hotkey)
{
    UINT modifiers;

    if (window == NULL || cf_hotkey_validate(&hotkey) != CF_OK) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    modifiers = (UINT)hotkey.modifiers | MOD_NOREPEAT;
    return RegisterHotKey(window, identifier, modifiers,
                          (UINT)hotkey.virtual_key)
               ? CF_OK
               : CF_ERR_CONFLICT;
}

void cf_win32_hotkey_unregister(HWND window, int identifier)
{
    if (window != NULL) {
        UnregisterHotKey(window, identifier);
    }
}
