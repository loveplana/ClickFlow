#include "core/clicker.h"
#include "test.h"

static CfVirtualScreen test_screen(void)
{
    CfVirtualScreen screen = {-1920, 0, 3840, 1080};
    return screen;
}

static CfClickerConfig valid_config(void)
{
    CfClickerConfig config = {0};
    config.button = CF_MOUSE_LEFT;
    config.click_kind = CF_CLICK_SINGLE;
    config.stop_mode = CF_STOP_MANUAL;
    config.position_mode = CF_POSITION_CURSOR;
    config.interval_ms = 100;
    return config;
}

static void test_accepts_single_and_double_click_modes(void)
{
    CfVirtualScreen screen = test_screen();
    CfClickerConfig config = valid_config();

    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen), CF_OK);
    config.click_kind = CF_CLICK_DOUBLE;
    config.button = CF_MOUSE_MIDDLE;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen), CF_OK);
    config.button = CF_MOUSE_RIGHT;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen), CF_OK);
}

static void test_rejects_interval_outside_range(void)
{
    CfVirtualScreen screen = test_screen();
    CfClickerConfig config = valid_config();

    config.interval_ms = 0;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    config.interval_ms = CF_CLICK_INTERVAL_MAX_MS + 1U;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen),
                      CF_ERR_INVALID_ARGUMENT);
}

static void test_count_stop_requires_bounded_count(void)
{
    CfVirtualScreen screen = test_screen();
    CfClickerConfig config = valid_config();

    config.stop_mode = CF_STOP_AFTER_COUNT;
    config.click_count = 0;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    config.click_count = CF_CLICK_COUNT_MAX + 1ULL;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    config.click_count = CF_CLICK_COUNT_MAX;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen), CF_OK);
}

static void test_duration_stop_requires_bounded_duration(void)
{
    CfVirtualScreen screen = test_screen();
    CfClickerConfig config = valid_config();

    config.stop_mode = CF_STOP_AFTER_DURATION;
    config.duration_ms = 0;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    config.duration_ms = CF_CLICK_DURATION_MAX_MS + 1U;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    config.duration_ms = CF_CLICK_DURATION_MAX_MS;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen), CF_OK);
}

static void test_fixed_position_must_be_on_virtual_screen(void)
{
    CfVirtualScreen screen = test_screen();
    CfClickerConfig config = valid_config();

    config.position_mode = CF_POSITION_FIXED;
    config.fixed_x = -1920;
    config.fixed_y = 1079;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen), CF_OK);
    config.fixed_x = 1920;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen),
                      CF_ERR_INVALID_ARGUMENT);
}

static void test_cursor_position_ignores_stored_fixed_coordinates(void)
{
    CfVirtualScreen screen = test_screen();
    CfClickerConfig config = valid_config();

    config.fixed_x = 900000;
    config.fixed_y = -900000;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen), CF_OK);
}

static void test_rejects_unknown_modes_and_buttons(void)
{
    CfVirtualScreen screen = test_screen();
    CfClickerConfig config = valid_config();

    config.button = (CfMouseButton)99;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    config = valid_config();
    config.click_kind = (CfClickKind)99;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    config = valid_config();
    config.stop_mode = (CfStopMode)99;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    config = valid_config();
    config.position_mode = (CfPositionMode)99;
    CF_TEST_ASSERT_EQ(cf_clicker_validate(&config, &screen),
                      CF_ERR_INVALID_ARGUMENT);
}

void cf_test_clicker(void)
{
    test_accepts_single_and_double_click_modes();
    test_rejects_interval_outside_range();
    test_count_stop_requires_bounded_count();
    test_duration_stop_requires_bounded_duration();
    test_fixed_position_must_be_on_virtual_screen();
    test_cursor_position_ignores_stored_fixed_coordinates();
    test_rejects_unknown_modes_and_buttons();
}
