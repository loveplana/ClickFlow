#include "core/clicker.h"
#include "core/playback.h"
#include "test.h"

#include <string.h>

#define FAKE_ACTION_CAPACITY 128

typedef struct FakeExecution {
    CfAction actions[FAKE_ACTION_CAPACITY];
    size_t action_count;
    uint32_t waits[FAKE_ACTION_CAPACITY];
    size_t wait_count;
    size_t cancel_on_wait;
    size_t fail_on_emit;
    uint64_t now_ms;
    int32_t cursor_x;
    int32_t cursor_y;
    size_t cursor_calls;
} FakeExecution;

static CfResult fake_emit(void *context, const CfAction *action)
{
    FakeExecution *fake = context;

    if (fake->fail_on_emit != 0 &&
        fake->action_count + 1U == fake->fail_on_emit) {
        fake->fail_on_emit = 0;
        return CF_ERR_PLATFORM;
    }
    CF_TEST_ASSERT(fake->action_count < FAKE_ACTION_CAPACITY);
    if (fake->action_count < FAKE_ACTION_CAPACITY) {
        fake->actions[fake->action_count++] = *action;
    }
    return CF_OK;
}

static CfWaitResult fake_wait(void *context, uint32_t milliseconds)
{
    FakeExecution *fake = context;

    CF_TEST_ASSERT(fake->wait_count < FAKE_ACTION_CAPACITY);
    if (fake->wait_count < FAKE_ACTION_CAPACITY) {
        fake->waits[fake->wait_count++] = milliseconds;
    }
    if (fake->cancel_on_wait != 0 && fake->wait_count == fake->cancel_on_wait) {
        return CF_WAIT_CANCELLED;
    }
    fake->now_ms += milliseconds;
    return CF_WAIT_ELAPSED;
}

static CfResult fake_cursor(void *context, int32_t *x, int32_t *y)
{
    FakeExecution *fake = context;
    ++fake->cursor_calls;
    *x = fake->cursor_x;
    *y = fake->cursor_y;
    return CF_OK;
}

static uint64_t fake_clock(void *context)
{
    return ((FakeExecution *)context)->now_ms;
}

static CfExecutionOps fake_ops(FakeExecution *fake)
{
    CfExecutionOps ops = {0};
    ops.context = fake;
    ops.emit = fake_emit;
    ops.wait_ms = fake_wait;
    ops.cursor_position = fake_cursor;
    ops.monotonic_ms = fake_clock;
    return ops;
}

static CfClickerConfig clicker_config(void)
{
    CfClickerConfig config = {0};
    config.button = CF_MOUSE_LEFT;
    config.click_kind = CF_CLICK_SINGLE;
    config.stop_mode = CF_STOP_AFTER_COUNT;
    config.position_mode = CF_POSITION_CURSOR;
    config.interval_ms = 100;
    config.click_count = 1;
    return config;
}

static void test_clicker_emits_single_and_double_clicks(void)
{
    FakeExecution fake = {0};
    CfExecutionOps ops = fake_ops(&fake);
    CfClickerConfig config = clicker_config();
    CfRunStats stats;

    fake.cursor_x = 400;
    fake.cursor_y = 300;
    CF_TEST_ASSERT_EQ(cf_clicker_run(&config, &ops, &stats), CF_OK);
    CF_TEST_ASSERT_EQ(fake.action_count, 1);
    CF_TEST_ASSERT_EQ(fake.actions[0].type, CF_ACTION_CLICK);
    CF_TEST_ASSERT_EQ(fake.actions[0].button, CF_MOUSE_LEFT);
    CF_TEST_ASSERT_EQ(stats.logical_clicks, 1);
    CF_TEST_ASSERT_EQ(fake.cursor_calls, 1);

    memset(&fake, 0, sizeof(fake));
    ops = fake_ops(&fake);
    config.click_kind = CF_CLICK_DOUBLE;
    config.button = CF_MOUSE_RIGHT;
    CF_TEST_ASSERT_EQ(cf_clicker_run(&config, &ops, &stats), CF_OK);
    CF_TEST_ASSERT_EQ(fake.action_count, 2);
    CF_TEST_ASSERT_EQ(fake.actions[0].button, CF_MOUSE_RIGHT);
    CF_TEST_ASSERT_EQ(fake.actions[1].button, CF_MOUSE_RIGHT);
}

static void test_clicker_honors_count_and_interval(void)
{
    FakeExecution fake = {0};
    CfExecutionOps ops = fake_ops(&fake);
    CfClickerConfig config = clicker_config();
    CfRunStats stats;

    config.click_count = 3;
    CF_TEST_ASSERT_EQ(cf_clicker_run(&config, &ops, &stats), CF_OK);
    CF_TEST_ASSERT_EQ(fake.action_count, 3);
    CF_TEST_ASSERT_EQ(fake.wait_count, 2);
    CF_TEST_ASSERT_EQ(fake.waits[0], 100);
    CF_TEST_ASSERT_EQ(stats.logical_clicks, 3);
}

static void test_clicker_stops_at_duration_boundary(void)
{
    FakeExecution fake = {0};
    CfExecutionOps ops = fake_ops(&fake);
    CfClickerConfig config = clicker_config();
    CfRunStats stats;

    config.stop_mode = CF_STOP_AFTER_DURATION;
    config.duration_ms = 250;
    CF_TEST_ASSERT_EQ(cf_clicker_run(&config, &ops, &stats), CF_OK);
    CF_TEST_ASSERT_EQ(fake.action_count, 3);
    CF_TEST_ASSERT_EQ(fake.wait_count, 3);
    CF_TEST_ASSERT_EQ(fake.waits[2], 50);
    CF_TEST_ASSERT_EQ(stats.elapsed_ms, 250);
}

static void test_clicker_cancellation_sends_nothing_after_cancel(void)
{
    FakeExecution fake = {0};
    CfExecutionOps ops = fake_ops(&fake);
    CfClickerConfig config = clicker_config();
    CfRunStats stats;

    config.stop_mode = CF_STOP_MANUAL;
    fake.cancel_on_wait = 1;
    CF_TEST_ASSERT_EQ(cf_clicker_run(&config, &ops, &stats),
                      CF_ERR_CANCELLED);
    CF_TEST_ASSERT_EQ(fake.action_count, 1);
    CF_TEST_ASSERT_EQ(fake.wait_count, 1);
}

static void test_fixed_position_moves_before_each_click(void)
{
    FakeExecution fake = {0};
    CfExecutionOps ops = fake_ops(&fake);
    CfClickerConfig config = clicker_config();
    CfRunStats stats;

    config.position_mode = CF_POSITION_FIXED;
    config.fixed_x = 80;
    config.fixed_y = 90;
    config.click_count = 2;
    CF_TEST_ASSERT_EQ(cf_clicker_run(&config, &ops, &stats), CF_OK);
    CF_TEST_ASSERT_EQ(fake.action_count, 4);
    CF_TEST_ASSERT_EQ(fake.actions[0].type, CF_ACTION_MOVE);
    CF_TEST_ASSERT_EQ(fake.actions[0].x, 80);
    CF_TEST_ASSERT_EQ(fake.actions[1].type, CF_ACTION_CLICK);
    CF_TEST_ASSERT_EQ(fake.actions[2].type, CF_ACTION_MOVE);
    CF_TEST_ASSERT_EQ(fake.cursor_calls, 0);
}

static void init_macro(CfMacro *macro)
{
    cf_macro_init(macro);
    CF_TEST_ASSERT_EQ(cf_macro_set_name(macro, "playback"), CF_OK);
    macro->recorded_screen = (CfVirtualScreen){0, 0, 1920, 1080};
}

static void test_macro_scales_waits_and_repeats(void)
{
    FakeExecution fake = {0};
    CfExecutionOps ops = fake_ops(&fake);
    CfRunOptions options = {false, 2, 2.0};
    CfRunStats stats;
    CfMacro macro;
    CfAction action = {0};

    init_macro(&macro);
    action.type = CF_ACTION_CLICK;
    action.button = CF_MOUSE_MIDDLE;
    action.delay_ms = 200;
    CF_TEST_ASSERT_EQ(cf_action_list_push(&macro.actions, action), CF_OK);
    CF_TEST_ASSERT_EQ(cf_macro_run(&macro, &options, &ops, &stats), CF_OK);
    CF_TEST_ASSERT_EQ(fake.wait_count, 2);
    CF_TEST_ASSERT_EQ(fake.waits[0], 100);
    CF_TEST_ASSERT_EQ(fake.action_count, 2);
    CF_TEST_ASSERT_EQ(stats.completed_repeats, 2);
    cf_macro_free(&macro);
}

static void test_macro_wait_action_does_not_emit_input(void)
{
    FakeExecution fake = {0};
    CfExecutionOps ops = fake_ops(&fake);
    CfRunOptions options = {false, 1, 1.0};
    CfRunStats stats;
    CfMacro macro;
    CfAction action = {0};

    init_macro(&macro);
    action.type = CF_ACTION_WAIT;
    action.delay_ms = 75;
    CF_TEST_ASSERT_EQ(cf_action_list_push(&macro.actions, action), CF_OK);
    CF_TEST_ASSERT_EQ(cf_macro_run(&macro, &options, &ops, &stats), CF_OK);
    CF_TEST_ASSERT_EQ(fake.wait_count, 1);
    CF_TEST_ASSERT_EQ(fake.waits[0], 75);
    CF_TEST_ASSERT_EQ(fake.action_count, 0);
    cf_macro_free(&macro);
}

static void test_macro_releases_pressed_button_when_wait_is_cancelled(void)
{
    FakeExecution fake = {0};
    CfExecutionOps ops = fake_ops(&fake);
    CfRunOptions options = {false, 1, 1.0};
    CfRunStats stats;
    CfMacro macro;
    CfAction down = {0};
    CfAction up = {0};

    init_macro(&macro);
    down.type = CF_ACTION_BUTTON_DOWN;
    down.button = CF_MOUSE_LEFT;
    up.type = CF_ACTION_BUTTON_UP;
    up.button = CF_MOUSE_LEFT;
    up.delay_ms = 600000;
    CF_TEST_ASSERT_EQ(cf_action_list_push(&macro.actions, down), CF_OK);
    CF_TEST_ASSERT_EQ(cf_action_list_push(&macro.actions, up), CF_OK);
    fake.cancel_on_wait = 1;

    CF_TEST_ASSERT_EQ(cf_macro_run(&macro, &options, &ops, &stats),
                      CF_ERR_CANCELLED);
    CF_TEST_ASSERT_EQ(fake.action_count, 2);
    CF_TEST_ASSERT_EQ(fake.actions[0].type, CF_ACTION_BUTTON_DOWN);
    CF_TEST_ASSERT_EQ(fake.actions[1].type, CF_ACTION_BUTTON_UP);
    CF_TEST_ASSERT_EQ(fake.wait_count, 1);
    cf_macro_free(&macro);
}

static void test_macro_releases_pressed_button_after_emit_error(void)
{
    FakeExecution fake = {0};
    CfExecutionOps ops = fake_ops(&fake);
    CfRunOptions options = {false, 1, 1.0};
    CfRunStats stats;
    CfMacro macro;
    CfAction down = {0};
    CfAction click = {0};
    CfAction up = {0};

    init_macro(&macro);
    down.type = CF_ACTION_BUTTON_DOWN;
    down.button = CF_MOUSE_RIGHT;
    click.type = CF_ACTION_CLICK;
    click.button = CF_MOUSE_LEFT;
    up.type = CF_ACTION_BUTTON_UP;
    up.button = CF_MOUSE_RIGHT;
    CF_TEST_ASSERT_EQ(cf_action_list_push(&macro.actions, down), CF_OK);
    CF_TEST_ASSERT_EQ(cf_action_list_push(&macro.actions, click), CF_OK);
    CF_TEST_ASSERT_EQ(cf_action_list_push(&macro.actions, up), CF_OK);
    fake.fail_on_emit = 2;

    CF_TEST_ASSERT_EQ(cf_macro_run(&macro, &options, &ops, &stats),
                      CF_ERR_PLATFORM);
    CF_TEST_ASSERT_EQ(fake.action_count, 2);
    CF_TEST_ASSERT_EQ(fake.actions[0].type, CF_ACTION_BUTTON_DOWN);
    CF_TEST_ASSERT_EQ(fake.actions[1].type, CF_ACTION_BUTTON_UP);
    cf_macro_free(&macro);
}

void cf_test_playback(void)
{
    test_clicker_emits_single_and_double_clicks();
    test_clicker_honors_count_and_interval();
    test_clicker_stops_at_duration_boundary();
    test_clicker_cancellation_sends_nothing_after_cancel();
    test_fixed_position_moves_before_each_click();
    test_macro_scales_waits_and_repeats();
    test_macro_wait_action_does_not_emit_input();
    test_macro_releases_pressed_button_when_wait_is_cancelled();
    test_macro_releases_pressed_button_after_emit_error();
}
