#include "core/recording.h"
#include "test.h"

#include <stdlib.h>

static CfVirtualScreen test_screen(void)
{
    CfVirtualScreen screen = {0, 0, 1920, 1080};
    return screen;
}

static CfRawMouseEvent raw_event(CfRawMouseEventType type, uint64_t time)
{
    CfRawMouseEvent event = {0};
    event.type = type;
    event.timestamp_ms = time;
    return event;
}

static void test_records_movement_and_preserves_elapsed_time(void)
{
    CfRecorderModel recorder;
    CfRawMouseEvent first = raw_event(CF_RAW_MOVE, 110);
    CfRawMouseEvent second = raw_event(CF_RAW_MOVE, 115);

    first.x = 10;
    first.y = 20;
    second.x = 30;
    second.y = 40;
    cf_recorder_init(&recorder, test_screen(), 100);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &first), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &second), CF_OK);
    CF_TEST_ASSERT_EQ(recorder.actions.count, 1);
    CF_TEST_ASSERT_EQ(recorder.actions.items[0].x, 30);
    CF_TEST_ASSERT_EQ(recorder.actions.items[0].y, 40);
    CF_TEST_ASSERT_EQ(recorder.actions.items[0].delay_ms, 15);
    cf_recorder_free(&recorder);
}

static void test_keeps_slow_or_interrupted_moves_separate(void)
{
    CfRecorderModel recorder;
    CfRawMouseEvent move1 = raw_event(CF_RAW_MOVE, 10);
    CfRawMouseEvent wheel = raw_event(CF_RAW_WHEEL, 14);
    CfRawMouseEvent move2 = raw_event(CF_RAW_MOVE, 18);
    CfRawMouseEvent move3 = raw_event(CF_RAW_MOVE, 40);

    move1.x = 1;
    move2.x = 2;
    move3.x = 3;
    wheel.wheel_delta = 120;
    cf_recorder_init(&recorder, test_screen(), 0);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &move1), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &wheel), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &move2), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &move3), CF_OK);
    CF_TEST_ASSERT_EQ(recorder.actions.count, 4);
    CF_TEST_ASSERT_EQ(recorder.actions.items[1].type, CF_ACTION_WHEEL);
    CF_TEST_ASSERT_EQ(recorder.actions.items[3].delay_ms, 22);
    cf_recorder_free(&recorder);
}

static void test_pairs_quick_down_up_as_click(void)
{
    CfRecorderModel recorder;
    CfRawMouseEvent down = raw_event(CF_RAW_BUTTON_DOWN, 100);
    CfRawMouseEvent up = raw_event(CF_RAW_BUTTON_UP, 130);

    down.button = CF_MOUSE_LEFT;
    up.button = CF_MOUSE_LEFT;
    cf_recorder_init(&recorder, test_screen(), 90);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &down), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &up), CF_OK);
    CF_TEST_ASSERT_EQ(recorder.actions.count, 1);
    CF_TEST_ASSERT_EQ(recorder.actions.items[0].type, CF_ACTION_CLICK);
    CF_TEST_ASSERT_EQ(recorder.actions.items[0].delay_ms, 10);
    cf_recorder_free(&recorder);
}

static void test_preserves_double_click_as_two_clicks(void)
{
    CfRecorderModel recorder;
    CfRawMouseEvent down1 = raw_event(CF_RAW_BUTTON_DOWN, 100);
    CfRawMouseEvent up1 = raw_event(CF_RAW_BUTTON_UP, 110);
    CfRawMouseEvent down2 = raw_event(CF_RAW_BUTTON_DOWN, 200);
    CfRawMouseEvent up2 = raw_event(CF_RAW_BUTTON_UP, 210);

    down1.button = up1.button = down2.button = up2.button = CF_MOUSE_LEFT;
    cf_recorder_init(&recorder, test_screen(), 100);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &down1), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &up1), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &down2), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &up2), CF_OK);
    CF_TEST_ASSERT_EQ(recorder.actions.count, 2);
    CF_TEST_ASSERT_EQ(recorder.actions.items[0].type, CF_ACTION_CLICK);
    CF_TEST_ASSERT_EQ(recorder.actions.items[1].type, CF_ACTION_CLICK);
    CF_TEST_ASSERT_EQ(recorder.actions.items[1].delay_ms, 90);
    cf_recorder_free(&recorder);
}

static void test_long_hold_remains_down_and_up(void)
{
    CfRecorderModel recorder;
    CfRawMouseEvent down = raw_event(CF_RAW_BUTTON_DOWN, 100);
    CfRawMouseEvent up = raw_event(CF_RAW_BUTTON_UP, 400);

    down.button = up.button = CF_MOUSE_RIGHT;
    cf_recorder_init(&recorder, test_screen(), 100);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &down), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &up), CF_OK);
    CF_TEST_ASSERT_EQ(recorder.actions.count, 2);
    CF_TEST_ASSERT_EQ(recorder.actions.items[0].type, CF_ACTION_BUTTON_DOWN);
    CF_TEST_ASSERT_EQ(recorder.actions.items[1].type, CF_ACTION_BUTTON_UP);
    CF_TEST_ASSERT_EQ(recorder.actions.items[1].delay_ms, 300);
    cf_recorder_free(&recorder);
}

static void test_pause_time_is_excluded_from_next_delay(void)
{
    CfRecorderModel recorder;
    CfRawMouseEvent move1 = raw_event(CF_RAW_MOVE, 20);
    CfRawMouseEvent move2 = raw_event(CF_RAW_MOVE, 1010);

    move1.x = 5;
    move2.x = 10;
    cf_recorder_init(&recorder, test_screen(), 0);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &move1), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_pause(&recorder, 100), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_resume(&recorder, 1000), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &move2), CF_OK);
    CF_TEST_ASSERT_EQ(recorder.actions.items[1].delay_ms, 10);
    cf_recorder_free(&recorder);
}

static void test_rejects_non_monotonic_and_paused_events(void)
{
    CfRecorderModel recorder;
    CfRawMouseEvent move = raw_event(CF_RAW_MOVE, 99);

    cf_recorder_init(&recorder, test_screen(), 100);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &move),
                      CF_ERR_INVALID_ARGUMENT);
    CF_TEST_ASSERT_EQ(cf_recorder_pause(&recorder, 100), CF_OK);
    move.timestamp_ms = 101;
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &move), CF_ERR_CONFLICT);
    cf_recorder_free(&recorder);
}

static void test_capacity_pressure_never_overwrites_non_move_event(void)
{
    CfRecorderModel recorder;
    CfRawMouseEvent down = raw_event(CF_RAW_BUTTON_DOWN, 1);

    down.button = CF_MOUSE_LEFT;
    cf_recorder_init(&recorder, test_screen(), 0);
    recorder.actions.items = malloc(sizeof(*recorder.actions.items));
    CF_TEST_ASSERT(recorder.actions.items != NULL);
    recorder.actions.items[0].type = CF_ACTION_WHEEL;
    recorder.actions.count = CF_ACTION_LIMIT;
    recorder.actions.capacity = CF_ACTION_LIMIT;
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &down), CF_ERR_LIMIT);
    CF_TEST_ASSERT_EQ(recorder.actions.items[0].type, CF_ACTION_WHEEL);
    recorder.actions.count = 1;
    recorder.actions.capacity = 1;
    cf_recorder_free(&recorder);
}

static void test_finish_copies_recording_into_valid_macro(void)
{
    CfRecorderModel recorder;
    CfRawMouseEvent wheel = raw_event(CF_RAW_WHEEL, 20);
    CfMacro macro;

    wheel.wheel_delta = -120;
    cf_recorder_init(&recorder, test_screen(), 0);
    cf_macro_init(&macro);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &wheel), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_finish(&recorder, "录制一", &macro), CF_OK);
    CF_TEST_ASSERT_EQ(cf_macro_validate(&macro, &macro.recorded_screen), CF_OK);
    CF_TEST_ASSERT_EQ(macro.actions.count, 1);
    CF_TEST_ASSERT_EQ(macro.actions.items[0].wheel_delta, -120);
    cf_macro_free(&macro);
    cf_recorder_free(&recorder);
}

static void test_finish_accepts_paused_recording(void)
{
    CfRecorderModel recorder;
    CfRawMouseEvent wheel = raw_event(CF_RAW_WHEEL, 20);
    CfMacro macro;

    wheel.wheel_delta = 120;
    cf_recorder_init(&recorder, test_screen(), 0);
    cf_macro_init(&macro);
    CF_TEST_ASSERT_EQ(cf_recorder_append(&recorder, &wheel), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_pause(&recorder, 25), CF_OK);
    CF_TEST_ASSERT_EQ(cf_recorder_finish(&recorder, "paused", &macro), CF_OK);
    CF_TEST_ASSERT_EQ(macro.actions.count, 1);
    cf_macro_free(&macro);
    cf_recorder_free(&recorder);
}

void cf_test_recording(void)
{
    test_records_movement_and_preserves_elapsed_time();
    test_keeps_slow_or_interrupted_moves_separate();
    test_pairs_quick_down_up_as_click();
    test_preserves_double_click_as_two_clicks();
    test_long_hold_remains_down_and_up();
    test_pause_time_is_excluded_from_next_delay();
    test_rejects_non_monotonic_and_paused_events();
    test_capacity_pressure_never_overwrites_non_move_event();
    test_finish_copies_recording_into_valid_macro();
    test_finish_accepts_paused_recording();
}
