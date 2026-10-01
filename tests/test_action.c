#include "core/action.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static CfVirtualScreen test_screen(void)
{
    CfVirtualScreen screen = {0, 0, 1920, 1080};
    return screen;
}

static void init_valid_macro(CfMacro *macro)
{
    CfAction wait_action = {0};

    cf_macro_init(macro);
    CF_TEST_ASSERT_EQ(cf_macro_set_name(macro, "示例宏"), CF_OK);
    macro->recorded_screen = test_screen();
    wait_action.type = CF_ACTION_WAIT;
    CF_TEST_ASSERT_EQ(cf_action_list_push(&macro->actions, wait_action), CF_OK);
}

static void test_action_list_grows_and_deep_copies(void)
{
    CfActionList source;
    CfActionList copy;
    CfAction action = {0};
    size_t index;

    cf_action_list_init(&source);
    cf_action_list_init(&copy);
    action.type = CF_ACTION_MOVE;

    for (index = 0; index < 40; ++index) {
        action.x = (int32_t)index;
        action.y = (int32_t)(index * 2U);
        CF_TEST_ASSERT_EQ(cf_action_list_push(&source, action), CF_OK);
    }

    CF_TEST_ASSERT_EQ(source.count, 40);
    CF_TEST_ASSERT(source.capacity >= source.count);
    CF_TEST_ASSERT_EQ(cf_action_list_copy(&copy, &source), CF_OK);
    CF_TEST_ASSERT_EQ(copy.count, source.count);
    source.items[0].x = 999;
    CF_TEST_ASSERT_EQ(copy.items[0].x, 0);

    cf_action_list_free(&copy);
    cf_action_list_free(&source);
}

static void test_action_limit_is_rejected_before_growth(void)
{
    CfActionList list = {0};
    CfAction action = {0};

    list.count = CF_ACTION_LIMIT;
    list.capacity = CF_ACTION_LIMIT;
    action.type = CF_ACTION_WAIT;
    CF_TEST_ASSERT_EQ(cf_action_list_push(&list, action), CF_ERR_LIMIT);
}

static void test_macro_copy_owns_name_and_actions(void)
{
    CfMacro source;
    CfMacro copy;

    init_valid_macro(&source);
    cf_macro_init(&copy);
    CF_TEST_ASSERT_EQ(cf_macro_copy(&copy, &source), CF_OK);
    CF_TEST_ASSERT(strcmp(copy.name, "示例宏") == 0);
    CF_TEST_ASSERT(copy.name != source.name);
    CF_TEST_ASSERT(copy.actions.items != source.actions.items);

    source.name[0] = 'X';
    source.actions.items[0].delay_ms = 75;
    CF_TEST_ASSERT(strcmp(copy.name, "示例宏") == 0);
    CF_TEST_ASSERT_EQ(copy.actions.items[0].delay_ms, 0);

    cf_macro_free(&copy);
    cf_macro_free(&source);
}

static void test_macro_rejects_invalid_button(void)
{
    CfMacro macro;
    CfVirtualScreen screen = test_screen();

    init_valid_macro(&macro);
    macro.actions.items[0].type = CF_ACTION_CLICK;
    macro.actions.items[0].button = (CfMouseButton)99;
    CF_TEST_ASSERT_EQ(cf_macro_validate(&macro, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    cf_macro_free(&macro);
}

static void test_macro_rejects_unreleased_button(void)
{
    CfMacro macro;
    CfVirtualScreen screen = test_screen();

    init_valid_macro(&macro);
    macro.actions.items[0].type = CF_ACTION_BUTTON_DOWN;
    macro.actions.items[0].button = CF_MOUSE_LEFT;
    CF_TEST_ASSERT_EQ(cf_macro_validate(&macro, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    cf_macro_free(&macro);
}

static void test_macro_rejects_button_up_without_down(void)
{
    CfMacro macro;
    CfVirtualScreen screen = test_screen();

    init_valid_macro(&macro);
    macro.actions.items[0].type = CF_ACTION_BUTTON_UP;
    macro.actions.items[0].button = CF_MOUSE_RIGHT;
    CF_TEST_ASSERT_EQ(cf_macro_validate(&macro, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    cf_macro_free(&macro);
}

static void test_macro_rejects_coordinate_outside_screen(void)
{
    CfMacro macro;
    CfVirtualScreen screen = test_screen();

    init_valid_macro(&macro);
    macro.actions.items[0].type = CF_ACTION_MOVE;
    macro.actions.items[0].x = 1920;
    macro.actions.items[0].y = 500;
    CF_TEST_ASSERT_EQ(cf_macro_validate(&macro, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    cf_macro_free(&macro);
}

static void test_macro_rejects_changed_screen_layout(void)
{
    CfMacro macro;
    CfVirtualScreen current = {0, 0, 2560, 1440};

    init_valid_macro(&macro);
    CF_TEST_ASSERT_EQ(cf_macro_validate(&macro, &current), CF_ERR_CONFLICT);
    cf_macro_free(&macro);
}

static void test_macro_rejects_delay_above_limit(void)
{
    CfMacro macro;
    CfVirtualScreen screen = test_screen();

    init_valid_macro(&macro);
    macro.actions.items[0].delay_ms = CF_ACTION_DELAY_MAX_MS + 1U;
    CF_TEST_ASSERT_EQ(cf_macro_validate(&macro, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    cf_macro_free(&macro);
}

static void test_macro_rejects_playback_speed_outside_range(void)
{
    CfMacro macro;
    CfVirtualScreen screen = test_screen();

    init_valid_macro(&macro);
    macro.playback_speed = 0.09;
    CF_TEST_ASSERT_EQ(cf_macro_validate(&macro, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    macro.playback_speed = 10.01;
    CF_TEST_ASSERT_EQ(cf_macro_validate(&macro, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    cf_macro_free(&macro);
}

static void test_macro_rejects_repeat_count_outside_range(void)
{
    CfMacro macro;
    CfVirtualScreen screen = test_screen();

    init_valid_macro(&macro);
    macro.repeat_count = 0;
    CF_TEST_ASSERT_EQ(cf_macro_validate(&macro, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    macro.repeat_count = CF_MACRO_REPEAT_MAX + 1U;
    CF_TEST_ASSERT_EQ(cf_macro_validate(&macro, &screen),
                      CF_ERR_INVALID_ARGUMENT);
    cf_macro_free(&macro);
}

static void test_valid_macro_is_accepted(void)
{
    CfMacro macro;
    CfVirtualScreen screen = test_screen();

    init_valid_macro(&macro);
    CF_TEST_ASSERT_EQ(cf_macro_validate(&macro, &screen), CF_OK);
    cf_macro_free(&macro);
}

void cf_test_action(void)
{
    test_action_list_grows_and_deep_copies();
    test_action_limit_is_rejected_before_growth();
    test_macro_copy_owns_name_and_actions();
    test_macro_rejects_invalid_button();
    test_macro_rejects_unreleased_button();
    test_macro_rejects_button_up_without_down();
    test_macro_rejects_coordinate_outside_screen();
    test_macro_rejects_changed_screen_layout();
    test_macro_rejects_delay_above_limit();
    test_macro_rejects_playback_speed_outside_range();
    test_macro_rejects_repeat_count_outside_range();
    test_valid_macro_is_accepted();
}
