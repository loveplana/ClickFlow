#include "runtime/controller.h"
#include "test.h"

#include <windows.h>

typedef struct FakeControllerInput {
    CfController *controller;
    volatile LONG emit_count;
    volatile LONG fail_on_emit;
} FakeControllerInput;

static CfResult fake_emit(void *context, const CfAction *action)
{
    FakeControllerInput *fake = context;
    LONG count = InterlockedIncrement(&fake->emit_count);
    (void)action;
    if (fake->fail_on_emit != 0 && count == fake->fail_on_emit) {
        return CF_ERR_PLATFORM;
    }
    return CF_OK;
}

static CfWaitResult fake_wait(void *context, uint32_t milliseconds)
{
    FakeControllerInput *fake = context;
    DWORD result = WaitForSingleObject(cf_controller_stop_event(fake->controller),
                                       milliseconds);
    if (result == WAIT_OBJECT_0) {
        return CF_WAIT_CANCELLED;
    }
    return result == WAIT_TIMEOUT ? CF_WAIT_ELAPSED : CF_WAIT_ERROR;
}

static CfResult fake_cursor(void *context, int32_t *x, int32_t *y)
{
    (void)context;
    *x = 100;
    *y = 100;
    return CF_OK;
}

static uint64_t fake_clock(void *context)
{
    (void)context;
    return GetTickCount64();
}

static CfExecutionOps fake_ops(FakeControllerInput *fake)
{
    CfExecutionOps ops = {0};
    ops.context = fake;
    ops.emit = fake_emit;
    ops.wait_ms = fake_wait;
    ops.cursor_position = fake_cursor;
    ops.monotonic_ms = fake_clock;
    return ops;
}

static CfClickerConfig count_job(uint64_t count)
{
    CfClickerConfig config = {0};
    config.button = CF_MOUSE_LEFT;
    config.click_kind = CF_CLICK_SINGLE;
    config.stop_mode = CF_STOP_AFTER_COUNT;
    config.position_mode = CF_POSITION_CURSOR;
    config.interval_ms = 10;
    config.click_count = count;
    return config;
}

static CfClickerConfig long_job(void)
{
    CfClickerConfig config = count_job(1000);
    config.interval_ms = 600000;
    return config;
}

static bool wait_for_state(CfController *controller, CfTaskState expected,
                           DWORD timeout_ms)
{
    uint64_t deadline = GetTickCount64() + timeout_ms;
    do {
        if (cf_controller_state(controller) == expected) {
            return true;
        }
        Sleep(1);
    } while (GetTickCount64() < deadline);
    return cf_controller_state(controller) == expected;
}

static CfController *test_controller(FakeControllerInput *fake)
{
    CfController *controller = cf_controller_create(NULL, 0);
    CF_TEST_ASSERT(controller != NULL);
    fake->controller = controller;
    CF_TEST_ASSERT_EQ(cf_controller_set_execution_ops(controller,
                                                      fake_ops(fake)), CF_OK);
    return controller;
}

static void test_completed_job_returns_to_idle(void)
{
    FakeControllerInput fake = {0};
    CfController *controller = test_controller(&fake);
    CfClickerConfig config = count_job(1);

    CF_TEST_ASSERT_EQ(cf_controller_start_clicker(controller, &config), CF_OK);
    CF_TEST_ASSERT(wait_for_state(controller, CF_TASK_IDLE, 1000));
    CF_TEST_ASSERT_EQ(fake.emit_count, 1);
    CF_TEST_ASSERT_EQ(cf_controller_last_result(controller), CF_OK);
    cf_controller_destroy(controller);
}

static void test_controller_rejects_second_activity(void)
{
    FakeControllerInput fake = {0};
    CfController *controller = test_controller(&fake);
    CfClickerConfig config = long_job();

    CF_TEST_ASSERT_EQ(cf_controller_start_clicker(controller, &config), CF_OK);
    CF_TEST_ASSERT(wait_for_state(controller, CF_TASK_RUNNING, 1000));
    CF_TEST_ASSERT_EQ(cf_controller_start_clicker(controller, &config),
                      CF_ERR_CONFLICT);
    CF_TEST_ASSERT_EQ(cf_controller_stop(controller), CF_OK);
    CF_TEST_ASSERT(wait_for_state(controller, CF_TASK_IDLE, 1000));
    CF_TEST_ASSERT_EQ(fake.emit_count, 1);
    cf_controller_destroy(controller);
}

static void test_pause_resume_and_stop_state_transitions(void)
{
    FakeControllerInput fake = {0};
    CfController *controller = test_controller(&fake);
    CfClickerConfig config = long_job();

    CF_TEST_ASSERT_EQ(cf_controller_start_clicker(controller, &config), CF_OK);
    CF_TEST_ASSERT(wait_for_state(controller, CF_TASK_RUNNING, 1000));
    CF_TEST_ASSERT_EQ(cf_controller_pause(controller), CF_OK);
    CF_TEST_ASSERT_EQ(cf_controller_state(controller), CF_TASK_PAUSED);
    CF_TEST_ASSERT_EQ(cf_controller_resume(controller), CF_OK);
    CF_TEST_ASSERT_EQ(cf_controller_state(controller), CF_TASK_RUNNING);
    CF_TEST_ASSERT_EQ(cf_controller_stop(controller), CF_OK);
    CF_TEST_ASSERT(wait_for_state(controller, CF_TASK_IDLE, 1000));
    cf_controller_destroy(controller);
}

static void test_partial_send_enters_error_state(void)
{
    FakeControllerInput fake = {0};
    CfController *controller = test_controller(&fake);
    CfClickerConfig config = count_job(1);

    fake.fail_on_emit = 1;
    CF_TEST_ASSERT_EQ(cf_controller_start_clicker(controller, &config), CF_OK);
    CF_TEST_ASSERT(wait_for_state(controller, CF_TASK_ERROR, 1000));
    CF_TEST_ASSERT_EQ(cf_controller_last_result(controller), CF_ERR_PLATFORM);
    CF_TEST_ASSERT_EQ(cf_controller_stop(controller), CF_OK);
    CF_TEST_ASSERT_EQ(cf_controller_state(controller), CF_TASK_IDLE);
    cf_controller_destroy(controller);
}

static void test_shutdown_interrupts_long_wait_promptly(void)
{
    FakeControllerInput fake = {0};
    CfController *controller = test_controller(&fake);
    CfClickerConfig config = long_job();
    uint64_t started;

    CF_TEST_ASSERT_EQ(cf_controller_start_clicker(controller, &config), CF_OK);
    CF_TEST_ASSERT(wait_for_state(controller, CF_TASK_RUNNING, 1000));
    started = GetTickCount64();
    cf_controller_shutdown(controller);
    CF_TEST_ASSERT(GetTickCount64() - started < 500);
    CF_TEST_ASSERT_EQ(cf_controller_state(controller), CF_TASK_IDLE);
    cf_controller_destroy(controller);
}

void cf_test_controller(void)
{
    test_completed_job_returns_to_idle();
    test_controller_rejects_second_activity();
    test_pause_resume_and_stop_state_transitions();
    test_partial_send_enters_error_state();
    test_shutdown_interrupts_long_wait_promptly();
}
