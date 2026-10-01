#include "core/hotkey.h"
#include "test.h"

#include <string.h>

enum {
    TEST_VK_ESCAPE = 0x1B,
    TEST_VK_CONTROL = 0x11,
    TEST_VK_F8 = 0x77
};

static void test_accepts_escape_f8_and_modified_f8(void)
{
    CfHotkey escape = {0, TEST_VK_ESCAPE};
    CfHotkey f8 = {0, TEST_VK_F8};
    CfHotkey modified = {CF_HOTKEY_CONTROL | CF_HOTKEY_ALT, TEST_VK_F8};

    CF_TEST_ASSERT_EQ(cf_hotkey_validate(&escape), CF_OK);
    CF_TEST_ASSERT_EQ(cf_hotkey_validate(&f8), CF_OK);
    CF_TEST_ASSERT_EQ(cf_hotkey_validate(&modified), CF_OK);
}

static void test_rejects_missing_key_and_modifier_key(void)
{
    CfHotkey missing = {CF_HOTKEY_CONTROL, 0};
    CfHotkey modifier = {0, TEST_VK_CONTROL};

    CF_TEST_ASSERT_EQ(cf_hotkey_validate(&missing), CF_ERR_INVALID_ARGUMENT);
    CF_TEST_ASSERT_EQ(cf_hotkey_validate(&modifier), CF_ERR_INVALID_ARGUMENT);
}

static void test_rejects_unknown_modifier_bits(void)
{
    CfHotkey hotkey = {0x1000U, TEST_VK_F8};
    CF_TEST_ASSERT_EQ(cf_hotkey_validate(&hotkey), CF_ERR_INVALID_ARGUMENT);
}

static void test_rejects_duplicate_primary_and_emergency(void)
{
    CfHotkey primary = {0, TEST_VK_F8};
    CfHotkey emergency = {0, TEST_VK_F8};

    CF_TEST_ASSERT(cf_hotkey_equal(primary, emergency));
    CF_TEST_ASSERT_EQ(cf_hotkey_validate_pair(primary, emergency),
                      CF_ERR_CONFLICT);
}

static void test_formats_common_hotkeys(void)
{
    char buffer[64];
    CfHotkey escape = {0, TEST_VK_ESCAPE};
    CfHotkey modified = {CF_HOTKEY_CONTROL | CF_HOTKEY_ALT, TEST_VK_F8};

    CF_TEST_ASSERT_EQ(cf_hotkey_format_utf8(escape, buffer, sizeof(buffer)),
                      CF_OK);
    CF_TEST_ASSERT(strcmp(buffer, "Esc") == 0);
    CF_TEST_ASSERT_EQ(cf_hotkey_format_utf8(modified, buffer, sizeof(buffer)),
                      CF_OK);
    CF_TEST_ASSERT(strcmp(buffer, "Ctrl+Alt+F8") == 0);
    CF_TEST_ASSERT_EQ(cf_hotkey_format_utf8(modified, buffer, 4),
                      CF_ERR_LIMIT);
}

void cf_test_hotkey(void)
{
    test_accepts_escape_f8_and_modified_f8();
    test_rejects_missing_key_and_modifier_key();
    test_rejects_unknown_modifier_bits();
    test_rejects_duplicate_primary_and_emergency();
    test_formats_common_hotkeys();
}
