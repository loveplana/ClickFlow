# ClickFlow V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a portable Windows 10/11 x64 application that provides mouse auto-clicking, mouse recording/playback, visual macros, global hotkeys, local JSON storage, and a polished bilingual-safe GUI.

**Architecture:** Keep the Windows/Nuklear shell thin and put validation, action modeling, recording normalization, scheduling, and JSON conversion behind small C interfaces. A single controller owns task state and runs either clicker playback or macro playback on one interruptible worker; the recorder owns a separate hook thread and feeds the same action model used by macros.

**Tech Stack:** C17, Win32 API, Direct3D 11, Nuklear v4.13.3, cJSON v1.7.19, CMake 4.x, Ninja, MinGW-w64 GCC 15.2, CTest.

**Spec:** `docs/superpowers/specs/2026-09-30-clickflow-design.md`

## Global Constraints

- Target Windows 10 and Windows 11 x64; do not require administrator rights.
- Use C17 and compile project code with GCC 15.2 using `-Wall -Wextra -Wpedantic -Werror`.
- The Release main program is one EXE; distribute third-party licenses and the usage guide beside it.
- Keep V1 offline: no network, account, telemetry, updater, AI API, arbitrary scripting, image recognition, background-window input, or keyboard macro input.
- Do not write the registry, install a service, add startup entries, or execute input automatically at launch.
- Only one of clicker, recorder, or macro playback may be active at once.
- All waits used by running tasks must be interruptible; closing the window must stop workers and uninstall hooks before process exit.
- Store configuration under `%LOCALAPPDATA%\ClickFlow`; user macro and recording paths remain user-selected.
- JSON files are UTF-8 and versioned with `schema_version: 1`; write files atomically.
- Import at most 64 MiB and 1,000,000 actions per recording or macro.
- Mouse coordinates use DPI-aware virtual-desktop physical pixels and must be revalidated before execution.
- Keep the default emergency stop hotkey available at all times; default to `Esc`.
- Preserve the V2 direction without implementing it: an AI API may later analyze user-selected recordings, but V1 adds no network layer or speculative provider abstraction.

## Review Focus

- **Stop during a long wait:** stopping a task with a multi-minute delay must return the controller to idle promptly and send no later input; Task 5 adds the cancellation test.
- **Partial `SendInput`:** if Windows reports fewer sent events than requested, playback must enter error state and release owned buttons; Task 7 adds an injected partial-send integration test.
- **Monitor layout changed:** a recording made against another virtual desktop must be rejected before playback; Tasks 2 and 4 test the mismatch.
- **Hook queue pressure:** rapid mouse movement must coalesce safely without losing button or wheel events; Task 6 adds saturation and ordering tests.
- **Damaged or oversized JSON:** loading must fail without replacing or mutating the existing in-memory macro; Task 4 tests truncation, excessive actions, excessive bytes, and unknown schema.

---

## Planned File Structure

```text
CMakeLists.txt
cmake/CompilerWarnings.cmake
resources/clickflow.manifest
resources/clickflow.rc
src/main.c
src/core/cf_result.h
src/core/action.h
src/core/action.c
src/core/hotkey.h
src/core/hotkey.c
src/core/clicker.h
src/core/clicker.c
src/core/playback.h
src/core/playback.c
src/core/recording.h
src/core/recording.c
src/core/config.h
src/core/config.c
src/storage/json_store.h
src/storage/json_store.c
src/runtime/controller.h
src/runtime/controller_win32.c
src/platform/win32_input.h
src/platform/win32_input.c
src/platform/win32_hotkey.h
src/platform/win32_hotkey.c
src/platform/win32_recorder.h
src/platform/win32_recorder.c
src/platform/win32_paths.h
src/platform/win32_paths.c
src/platform/win32_single_instance.h
src/platform/win32_single_instance.c
src/ui/app.h
src/ui/app.c
src/ui/theme.h
src/ui/theme.c
src/ui/page_clicker.c
src/ui/page_recording.c
src/ui/page_macro.c
src/ui/page_settings.c
src/ui/widgets.c
src/ui/widgets.h
tests/test.h
tests/test_main.c
tests/test_action.c
tests/test_hotkey.c
tests/test_clicker.c
tests/test_playback.c
tests/test_recording.c
tests/test_storage.c
tests/test_controller.c
third_party/nuklear/nuklear.h
third_party/nuklear/nuklear_d3d11.h
third_party/nuklear/LICENSE.md
third_party/cjson/cJSON.h
third_party/cjson/cJSON.c
third_party/cjson/LICENSE
third_party/README.md
docs/ROADMAP.md
README.md
LICENSES.md
```

## Task 1: Reproducible Build, Vendored Libraries, and Test Harness

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/CompilerWarnings.cmake`
- Create: `src/main.c`
- Create: `src/core/cf_result.h`
- Create: `tests/test.h`
- Create: `tests/test_main.c`
- Create: `third_party/README.md`
- Vendor: `third_party/nuklear/nuklear.h`
- Vendor: `third_party/nuklear/nuklear_d3d11.h`
- Vendor: `third_party/nuklear/LICENSE.md`
- Vendor: `third_party/cjson/cJSON.h`
- Vendor: `third_party/cjson/cJSON.c`
- Vendor: `third_party/cjson/LICENSE`

**Interfaces:**
- Produces: `CfResult` error enum, `CF_ARRAY_COUNT`, and a test runner used by every later task.
- Consumes: no project interfaces.

- [ ] **Step 1: Vendor exact upstream releases**

Download the official `Nuklear v4.13.3` and `cJSON v1.7.19` release archives. Copy only `nuklear.h`, `demo/d3d11/nuklear_d3d11.h`, and the Nuklear license; copy only `cJSON.c`, `cJSON.h`, and the cJSON license. Record the upstream URLs, tag names, and SHA-256 hashes of copied source files in `third_party/README.md`.

Official archives:

```text
https://github.com/Immediate-Mode-UI/Nuklear/archive/refs/tags/v4.13.3.zip
https://github.com/DaveGamble/cJSON/archive/refs/tags/v1.7.19.zip
```

- [ ] **Step 2: Add the shared result type and a failing smoke test**

Create `src/core/cf_result.h`:

```c
#ifndef CLICKFLOW_CF_RESULT_H
#define CLICKFLOW_CF_RESULT_H

typedef enum CfResult {
    CF_OK = 0,
    CF_ERR_INVALID_ARGUMENT,
    CF_ERR_OUT_OF_MEMORY,
    CF_ERR_LIMIT,
    CF_ERR_IO,
    CF_ERR_FORMAT,
    CF_ERR_SCHEMA,
    CF_ERR_CONFLICT,
    CF_ERR_PLATFORM,
    CF_ERR_CANCELLED
} CfResult;

#define CF_ARRAY_COUNT(values) (sizeof(values) / sizeof((values)[0]))

#endif
```

Create a tiny assertion runner in `tests/test.h` with `CF_TEST_ASSERT`, `CF_TEST_ASSERT_EQ`, and a global failure count. Make `tests/test_main.c` assert that `CF_OK == 0` and intentionally invert that expectation for the first run.

- [ ] **Step 3: Add CMake targets and verify the test fails**

Create `clickflow_core`, `clickflow_tests`, and a temporary console `clickflow` target. Enable C17, warnings-as-errors for project sources, CTest, and Windows system libraries `user32`, `shell32`, `ole32`, `d3d11`, `dxgi`, `d3dcompiler`, and `comdlg32` for the app target. Do not apply project warning flags to vendored cJSON.

Run:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=C:/mingw64/bin/gcc.exe -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: build succeeds and `clickflow_tests` fails at the intentionally inverted assertion.

- [ ] **Step 4: Correct the smoke test and verify the toolchain**

Change the assertion to `CF_TEST_ASSERT_EQ(CF_OK, 0)`. Rebuild and run CTest.

Expected: one test target passes; `CMakeCache.txt` reports `C:/mingw64/bin/gcc.exe` rather than the older GCC 4.9.2 earlier on `PATH`.

- [ ] **Step 5: Commit**

```powershell
git add CMakeLists.txt cmake src/core/cf_result.h src/main.c tests third_party
git commit -m "build: establish ClickFlow C17 project"
```

## Task 2: Action Model and Validation

**Files:**
- Create: `src/core/action.h`
- Create: `src/core/action.c`
- Create: `tests/test_action.c`
- Modify: `tests/test_main.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `CfAction`, `CfActionList`, `CfMacro`, `CfVirtualScreen`, `cf_action_list_*`, `cf_macro_*`, and `cf_macro_validate`.
- Consumes: `CfResult` from Task 1.

- [ ] **Step 1: Write failing action-list tests**

Cover initialization, capacity growth, one-million-action rejection, deep copy independence, invalid mouse buttons, unmatched button-down, coordinate bounds, screen-layout mismatch, delay above `2592000000`, playback speed outside `0.1..10.0`, and repeat counts outside `1..1000000`.

Use these public types in the tests:

```c
typedef enum CfActionType {
    CF_ACTION_MOVE,
    CF_ACTION_BUTTON_DOWN,
    CF_ACTION_BUTTON_UP,
    CF_ACTION_CLICK,
    CF_ACTION_WHEEL,
    CF_ACTION_WAIT
} CfActionType;

typedef enum CfMouseButton {
    CF_MOUSE_LEFT,
    CF_MOUSE_MIDDLE,
    CF_MOUSE_RIGHT
} CfMouseButton;

typedef struct CfVirtualScreen {
    int32_t left, top, width, height;
} CfVirtualScreen;

typedef struct CfAction {
    CfActionType type;
    uint32_t delay_ms;
    int32_t x, y;
    int32_t wheel_delta;
    CfMouseButton button;
} CfAction;
```

- [ ] **Step 2: Run the action tests and confirm missing symbols**

Run `cmake --build build && ctest --test-dir build --output-on-failure`.

Expected: link or compile failure for the undeclared action-list and macro functions.

- [ ] **Step 3: Implement bounded ownership and validation**

Expose these signatures from `action.h`:

```c
void cf_action_list_init(CfActionList *list);
void cf_action_list_free(CfActionList *list);
CfResult cf_action_list_push(CfActionList *list, CfAction action);
CfResult cf_action_list_copy(CfActionList *dst, const CfActionList *src);
void cf_macro_init(CfMacro *macro);
void cf_macro_free(CfMacro *macro);
CfResult cf_macro_copy(CfMacro *dst, const CfMacro *src);
CfResult cf_macro_validate(const CfMacro *macro,
                           const CfVirtualScreen *current_screen);
```

Use checked doubling for capacity, stop at `1,000,000`, keep `CfMacro.name` as an owned UTF-8 allocation capped at 255 bytes, and leave the destination unchanged when a copy fails.

- [ ] **Step 4: Run focused and full tests**

Run `build\clickflow_tests.exe action`, then `ctest --test-dir build --output-on-failure`.

Expected: all action tests pass under warnings-as-errors.

- [ ] **Step 5: Commit**

```powershell
git add src/core/action.c src/core/action.h tests/test_action.c tests/test_main.c CMakeLists.txt
git commit -m "feat: add validated mouse action model"
```

## Task 3: Hotkey and Clicker Domain Logic

**Files:**
- Create: `src/core/hotkey.h`
- Create: `src/core/hotkey.c`
- Create: `src/core/clicker.h`
- Create: `src/core/clicker.c`
- Create: `tests/test_hotkey.c`
- Create: `tests/test_clicker.c`
- Modify: `tests/test_main.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `CfHotkey`, `cf_hotkey_validate`, `cf_hotkey_equal`, `cf_hotkey_format_utf8`, `CfClickerConfig`, and `cf_clicker_validate`.
- Consumes: `CfMouseButton`, `CfVirtualScreen`, and `CfResult`.

- [ ] **Step 1: Write failing hotkey and clicker validation tests**

Test unmodified `Esc`, `F8`, `Ctrl+Alt+F8`, duplicate emergency/primary hotkeys, modifier-only input, interval bounds, click-count bounds, duration bounds, fixed coordinates, current-cursor mode, and single/double click mode.

The expected public configuration is:

```c
typedef enum CfClickKind { CF_CLICK_SINGLE, CF_CLICK_DOUBLE } CfClickKind;
typedef enum CfStopMode { CF_STOP_MANUAL, CF_STOP_AFTER_COUNT, CF_STOP_AFTER_DURATION } CfStopMode;
typedef enum CfPositionMode { CF_POSITION_CURSOR, CF_POSITION_FIXED } CfPositionMode;

typedef struct CfClickerConfig {
    CfMouseButton button;
    CfClickKind click_kind;
    CfStopMode stop_mode;
    CfPositionMode position_mode;
    uint32_t interval_ms;
    uint64_t click_count;
    uint32_t duration_ms;
    int32_t fixed_x;
    int32_t fixed_y;
} CfClickerConfig;
```

- [ ] **Step 2: Run tests and confirm they fail for missing implementations**

Run `cmake --build build`.

Expected: compile or link failure naming the new APIs.

- [ ] **Step 3: Implement normalization and validation**

Represent hotkeys as a modifier bitmask plus Win32 virtual-key value without including `windows.h` in the public core header. Format common modifier and function keys in UTF-8. Reject `0`, modifier-only values, and equality with a reserved hotkey.

Implement:

```c
CfResult cf_hotkey_validate(const CfHotkey *hotkey);
bool cf_hotkey_equal(CfHotkey a, CfHotkey b);
CfResult cf_hotkey_format_utf8(CfHotkey hotkey, char *buffer, size_t size);
CfResult cf_clicker_validate(const CfClickerConfig *config,
                             const CfVirtualScreen *screen);
```

- [ ] **Step 4: Run focused and full tests**

Run `build\clickflow_tests.exe hotkey clicker` and then CTest.

Expected: all tests pass.

- [ ] **Step 5: Commit**

```powershell
git add src/core/hotkey.c src/core/hotkey.h src/core/clicker.c src/core/clicker.h tests CMakeLists.txt
git commit -m "feat: define hotkey and clicker rules"
```

## Task 4: Versioned JSON Configuration, Macro, and Recording Storage

**Files:**
- Create: `src/core/config.h`
- Create: `src/core/config.c`
- Create: `src/storage/json_store.h`
- Create: `src/storage/json_store.c`
- Create: `tests/test_storage.c`
- Modify: `tests/test_main.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `CfConfig`, `cf_config_defaults`, `cf_json_load_config`, `cf_json_save_config_atomic`, `cf_json_load_macro`, and `cf_json_save_macro_atomic`.
- Consumes: cJSON, `CfMacro`, `CfHotkey`, `CfClickerConfig`, and `CfResult`.

- [ ] **Step 1: Write failing round-trip and hostile-file tests**

Add tests for default config, full config round trip, the exact schema example in the design spec, UTF-8 Chinese macro names, unknown schema, wrong `kind`, truncated JSON, 64 MiB + 1 byte input, 1,000,001 actions, layout mismatch validation after load, unwritable destination, and preservation of the caller's existing macro when load fails.

- [ ] **Step 2: Run tests and confirm storage APIs are missing**

Run `cmake --build build`.

Expected: compile or link failure for `cf_json_*` functions.

- [ ] **Step 3: Implement strict parsing and atomic save**

Expose:

```c
CfResult cf_json_load_config(const wchar_t *path, CfConfig *out,
                             char *error, size_t error_size);
CfResult cf_json_save_config_atomic(const wchar_t *path,
                                    const CfConfig *config,
                                    char *error, size_t error_size);
CfResult cf_json_load_macro(const wchar_t *path, const char *expected_kind,
                            CfMacro *out, char *error, size_t error_size);
CfResult cf_json_save_macro_atomic(const wchar_t *path, const char *kind,
                                   const CfMacro *macro,
                                   char *error, size_t error_size);
```

Read file size before allocation, parse into a temporary object, reject duplicate required keys, validate numeric ranges before narrowing, and swap into the output only after full validation. Save beside the target as `.tmp`, call `FlushFileBuffers`, then use `ReplaceFileW` or `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.

- [ ] **Step 4: Run storage and full tests**

Run `build\clickflow_tests.exe storage` and CTest.

Expected: all valid files round-trip byte-semantically and all hostile inputs fail without mutating existing state.

- [ ] **Step 5: Commit**

```powershell
git add src/core/config.c src/core/config.h src/storage tests/test_storage.c tests/test_main.c CMakeLists.txt
git commit -m "feat: persist versioned ClickFlow data"
```

## Task 5: Interruptible Clicker and Macro Playback

**Files:**
- Create: `src/core/playback.h`
- Create: `src/core/playback.c`
- Create: `tests/test_playback.c`
- Modify: `src/core/clicker.c`
- Modify: `src/core/clicker.h`
- Modify: `tests/test_clicker.c`
- Modify: `tests/test_main.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `CfExecutionOps`, `CfRunOptions`, `CfRunStats`, `cf_clicker_run`, and `cf_macro_run`.
- Consumes: validated clicker and macro models.

- [ ] **Step 1: Write failing execution tests with fake time and input**

The fake captures emitted actions and returns either elapsed, cancelled, or platform error from waits. Cover single and double click ordering, left/middle/right buttons, count stop, duration stop, manual cancellation, fixed versus current position, playback speed scaling, repeat count, release of an owned button on cancellation, no event after cancellation, and cancellation during a multi-minute wait.

Use this seam:

```c
typedef enum CfWaitResult { CF_WAIT_ELAPSED, CF_WAIT_CANCELLED, CF_WAIT_ERROR } CfWaitResult;

typedef struct CfExecutionOps {
    void *context;
    CfResult (*emit)(void *context, const CfAction *action);
    CfWaitResult (*wait_ms)(void *context, uint32_t milliseconds);
    CfResult (*cursor_position)(void *context, int32_t *x, int32_t *y);
    uint64_t (*monotonic_ms)(void *context);
} CfExecutionOps;

typedef struct CfRunOptions {
    bool run_forever;
    uint32_t repeat_count;
    double playback_speed;
} CfRunOptions;
```

- [ ] **Step 2: Confirm playback tests fail**

Run `cmake --build build`.

Expected: missing playback symbols.

- [ ] **Step 3: Implement execution without Windows dependencies**

Implement:

```c
CfResult cf_clicker_run(const CfClickerConfig *config,
                        const CfExecutionOps *ops,
                        CfRunStats *stats);
CfResult cf_macro_run(const CfMacro *macro,
                      const CfRunOptions *options,
                      const CfExecutionOps *ops,
                      CfRunStats *stats);
```

Track buttons successfully pressed by ClickFlow and emit matching releases on all exits. Stop immediately on cancellation or emit error. Use monotonic elapsed time for duration mode, and clamp scaled waits to the validated integer range without overflow.

- [ ] **Step 4: Run playback and full tests**

Run `build\clickflow_tests.exe playback clicker` and CTest.

Expected: all execution sequences exactly match their expected action arrays.

- [ ] **Step 5: Commit**

```powershell
git add src/core/playback.c src/core/playback.h src/core/clicker.c src/core/clicker.h tests CMakeLists.txt
git commit -m "feat: execute interruptible click and macro jobs"
```

## Task 6: Recording Normalization and Queue-Pressure Behavior

**Files:**
- Create: `src/core/recording.h`
- Create: `src/core/recording.c`
- Create: `tests/test_recording.c`
- Modify: `tests/test_main.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `CfRawMouseEvent`, `CfRecorderModel`, `cf_recorder_append`, `cf_recorder_pause`, `cf_recorder_resume`, and `cf_recorder_finish`.
- Consumes: `CfActionList`, `CfVirtualScreen`, and `CfResult`.

- [ ] **Step 1: Write failing event-conversion tests**

Cover movement, button down/up, click pairing, double click preservation, wheel delta, pause/resume timing, non-monotonic timestamp rejection, continuous-move coalescing, button events between moves, queue saturation, and exactly one million output actions.

Use:

```c
typedef enum CfRawMouseEventType {
    CF_RAW_MOVE,
    CF_RAW_BUTTON_DOWN,
    CF_RAW_BUTTON_UP,
    CF_RAW_WHEEL
} CfRawMouseEventType;

typedef struct CfRawMouseEvent {
    CfRawMouseEventType type;
    uint64_t timestamp_ms;
    int32_t x, y;
    int32_t wheel_delta;
    CfMouseButton button;
} CfRawMouseEvent;
```

- [ ] **Step 2: Run tests and confirm the recorder functions are missing**

Run `cmake --build build`.

Expected: missing recorder symbols.

- [ ] **Step 3: Implement deterministic normalization**

Convert timestamp gaps into each action's `delay_ms`. Coalesce only adjacent move events that occur within 8 milliseconds and have no intervening button or wheel event; always retain the newest coordinates. When the queue is under pressure, overwrite a pending coalescible move, but never overwrite button or wheel events. Return `CF_ERR_LIMIT` and stop recording if a non-coalescible event cannot be retained.

- [ ] **Step 4: Run saturation and full tests**

Run `build\clickflow_tests.exe recording` and CTest.

Expected: ordering is stable, movement is reduced, and button/wheel events are never lost.

- [ ] **Step 5: Commit**

```powershell
git add src/core/recording.c src/core/recording.h tests/test_recording.c tests/test_main.c CMakeLists.txt
git commit -m "feat: normalize recorded mouse activity"
```

## Task 7: Win32 Input, Controller, Hotkeys, Recorder Hook, Paths, and Single Instance

**Files:**
- Create: `src/runtime/controller.h`
- Create: `src/runtime/controller_win32.c`
- Create: `src/platform/win32_input.h`
- Create: `src/platform/win32_input.c`
- Create: `src/platform/win32_hotkey.h`
- Create: `src/platform/win32_hotkey.c`
- Create: `src/platform/win32_recorder.h`
- Create: `src/platform/win32_recorder.c`
- Create: `src/platform/win32_paths.h`
- Create: `src/platform/win32_paths.c`
- Create: `src/platform/win32_single_instance.h`
- Create: `src/platform/win32_single_instance.c`
- Create: `tests/test_controller.c`
- Modify: `tests/test_main.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `CfController`, `cf_controller_start_clicker`, `cf_controller_start_macro`, `cf_controller_start_recording`, pause/resume/stop/shutdown functions, Win32 `CfExecutionOps`, hotkey registration, recorder hook lifecycle, known-folder paths, and single-instance activation.
- Consumes: all core interfaces from Tasks 2–6.

- [ ] **Step 1: Write failing controller state tests**

Test `IDLE -> STARTING -> RUNNING`, pause/resume, stop, error cleanup, refusal to start a second activity, shutdown while running, worker completion notification, and partial-send failure. Supply fake platform functions so automated tests never call real `SendInput` or install a hook.

Public controller API:

```c
typedef enum CfTaskState {
    CF_TASK_IDLE,
    CF_TASK_STARTING,
    CF_TASK_RUNNING,
    CF_TASK_PAUSED,
    CF_TASK_STOPPING,
    CF_TASK_ERROR
} CfTaskState;

CfResult cf_controller_start_clicker(CfController *, const CfClickerConfig *);
CfResult cf_controller_start_macro(CfController *, const CfMacro *, const CfRunOptions *);
CfResult cf_controller_start_recording(CfController *, const CfVirtualScreen *);
CfResult cf_controller_pause(CfController *);
CfResult cf_controller_resume(CfController *);
CfResult cf_controller_stop(CfController *);
void cf_controller_shutdown(CfController *);
CfTaskState cf_controller_state(const CfController *);
```

- [ ] **Step 2: Run tests and confirm controller symbols are missing**

Run `cmake --build build`.

Expected: missing controller symbols.

- [ ] **Step 3: Implement Windows adapters**

Map actions to `INPUT` records and use `MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_ABSOLUTE` for recorded coordinates. Use a manual-reset stop event plus a waitable timer through `WaitForMultipleObjects` for interruptible waits. Treat a partial `SendInput` return as `CF_ERR_PLATFORM`.

Use `RegisterHotKey` for global shortcuts, `WH_MOUSE_LL` on a dedicated message-loop thread for recording, `SHGetKnownFolderPath(FOLDERID_LocalAppData)` for data paths, and a named mutex plus registered window message for single-instance activation.

- [ ] **Step 4: Implement the controller and completion messages**

The controller owns copied job data, thread handles, stop/pause events, last error text, and state synchronization. Worker threads post a private `WM_APP` completion message to the UI window; they never call Nuklear or mutate UI fields.

- [ ] **Step 5: Run automated integration tests**

Run `build\clickflow_tests.exe controller` and CTest.

Expected: fake partial send produces `CF_TASK_ERROR`; cancellation during a long wait returns to idle without later emissions; shutdown joins all fake workers.

- [ ] **Step 6: Commit**

```powershell
git add src/runtime src/platform tests/test_controller.c tests/test_main.c CMakeLists.txt
git commit -m "feat: integrate ClickFlow with Windows input"
```

## Task 8: Window, Direct3D 11 Renderer, Fonts, Theme, and Shared Widgets

**Files:**
- Create: `resources/clickflow.manifest`
- Create: `resources/clickflow.rc`
- Create: `src/ui/app.h`
- Create: `src/ui/app.c`
- Create: `src/ui/theme.h`
- Create: `src/ui/theme.c`
- Create: `src/ui/widgets.h`
- Create: `src/ui/widgets.c`
- Modify: `src/main.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: DPI-aware app shell, `CfAppModel`, dark/light themes, navigation, cards, state badge, hotkey capture field, numeric field, primary button, toast, modal, and file dialog wrappers.
- Consumes: Controller, Config, Storage, Nuklear, D3D11, and Win32 adapters.

- [ ] **Step 1: Add the Windows manifest and window smoke path**

Declare Per-Monitor V2 DPI awareness and Windows 10/11 compatibility in `clickflow.manifest`. Add a Windows GUI subsystem target with `wWinMain`, create an 820 × 560 logical-pixel window, initialize D3D11, and render an empty Nuklear frame in the dark background color `#111318`.

- [ ] **Step 2: Build and manually verify shell behavior**

Run:

```powershell
cmake --build build
build\clickflow.exe
```

Expected: one resizable window opens, renders continuously without flicker, responds to close, and a second launch activates the first window.

- [ ] **Step 3: Load Chinese-capable fonts and implement themes**

Find the Windows font directory from `GetWindowsDirectoryW`. Prefer `msyh.ttc`, then `segoeui.ttf`. Bake Basic Latin plus Nuklear's Chinese glyph range. Define exact shared colors:

```text
Dark background #111318, card #1A1D24, raised #232833
Dark text #F4F6F8, muted #969EAA, border #303640
Light background #F3F5F7, card #FFFFFF, text #1A1D24
Accent #28C78B, accent hover #35D99A, danger #EF5B64
```

- [ ] **Step 4: Implement reusable widgets and navigation**

Create a 72-pixel left rail, a title/status header, 8-pixel-radius cards, segmented selectors, validated numeric inputs, hotkey capture, primary/danger buttons, toast messages, and confirmation modal. Use 100–180 millisecond state transitions driven by monotonic time; skip animation when Windows reduced-motion settings request it.

- [ ] **Step 5: Verify DPI and failure fallback**

Manually inspect 100%, 125%, 150%, and 200% DPI. Temporarily force preferred-font loading to fail and verify the fallback font appears without crashing. Resize down to the minimum supported client area and verify no controls overlap.

- [ ] **Step 6: Commit**

```powershell
git add resources src/ui src/main.c CMakeLists.txt
git commit -m "feat: add polished Nuklear application shell"
```

## Task 9: Clicker, Recording, Macro, and Settings Pages

**Files:**
- Create: `src/ui/page_clicker.c`
- Create: `src/ui/page_recording.c`
- Create: `src/ui/page_macro.c`
- Create: `src/ui/page_settings.c`
- Modify: `src/ui/app.h`
- Modify: `src/ui/app.c`
- Modify: `src/ui/widgets.h`
- Modify: `src/ui/widgets.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: all four approved screens and their controller/storage interactions.
- Consumes: shared widgets, validated domain types, storage, controller, file dialogs, and hotkey registration.

- [ ] **Step 1: Implement the clicker page**

Lay out three cards: click button/kind, interval/stop condition/position, and hotkey/run control. Bind visible validation messages to `cf_clicker_validate`. Capture fixed coordinates only after an explicit “取当前坐标” action. Change the primary control between “开始连点” and “停止”, and show `F8` in its label when it is the configured hotkey.

- [ ] **Step 2: Implement the recording page**

Provide record, pause, resume, stop, save, load, rename, delete, speed, and repeat controls. Show elapsed time and action count. Require confirmation before discarding an unsaved recording. Save with `.cfr.json` and `kind: "recording"`.

- [ ] **Step 3: Implement the macro page**

Provide a scrollable action table with type, delay, parameters, selection, add, duplicate, edit, delete, move up/down, load, save, speed, repeat, individual hotkey, run, pause, resume, and stop. Save with `.cfm.json` and `kind: "macro"`. Disable Run until `cf_macro_validate` succeeds.

- [ ] **Step 4: Implement settings and startup recovery**

Support theme, default clicker values, primary hotkey, and emergency hotkey. Save only validated settings. At startup load `%LOCALAPPDATA%\ClickFlow\config.json`; on parse failure, move it into `backups` with a timestamped filename, load defaults, and show a non-blocking warning.

- [ ] **Step 5: Wire global messages and state-dependent disabling**

Handle `WM_HOTKEY`, controller completion messages, recorder progress, and single-instance activation. While a task runs, disable operations that would violate task exclusivity. The emergency hotkey always calls controller stop before any page-specific behavior.

- [ ] **Step 6: Run the full automated suite and manual functional pass**

Run:

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
build\clickflow.exe
```

Expected: automated tests pass; every primary page flow works; no task starts on application launch; hotkeys work while Notepad is foreground; stopping leaves no pressed mouse button.

- [ ] **Step 7: Commit**

```powershell
git add src/ui CMakeLists.txt
git commit -m "feat: complete ClickFlow desktop workflows"
```

## Task 10: Release Build, Soak Checks, Documentation, and V2 Roadmap

**Files:**
- Create: `README.md`
- Create: `LICENSES.md`
- Create: `docs/ROADMAP.md`
- Modify: `CMakeLists.txt`
- Modify: any implementation file only when a release check exposes a defect

**Interfaces:**
- Produces: portable Release artifact, user instructions, dependency attributions, and a scoped V2 note.
- Consumes: the complete V1 application.

- [ ] **Step 1: Write user documentation and licenses**

Document build commands, the four pages, safe start/stop behavior, default `F8` and `Esc` controls, file extensions, data locations, portability, limitations, and uninstall-by-deletion. Attribute Nuklear v4.13.3 and cJSON v1.7.19 and link their included license files.

- [ ] **Step 2: Record the next-version direction without adding V1 code**

Create `docs/ROADMAP.md` with one V2 proposal: after explicit user selection and consent, serialize a recording into a minimized action summary, send it to a user-configured AI API, receive a versioned proposed macro, validate it locally, preview every generated action, and require confirmation before saving or running. State that provider choice, privacy/redaction, authentication storage, schema, costs, retry behavior, and prompt-injection handling require a separate design before implementation.

- [ ] **Step 3: Build and test Release with the required compiler**

Run:

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_C_COMPILER=C:/mingw64/bin/gcc.exe -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

Expected: zero project warnings, all tests pass, and `build-release\clickflow.exe` runs without non-system DLLs beside it.

- [ ] **Step 4: Verify the portable dependency surface**

Run `objdump -p build-release\clickflow.exe` and inspect `DLL Name` entries.

Expected: only Windows system libraries appear; no `libgcc_s_*.dll`, `libwinpthread-1.dll`, cJSON DLL, or Nuklear DLL is required.

- [ ] **Step 5: Execute the manual acceptance matrix**

Verify clicker, recording, playback, macros, global hotkeys, fixed coordinates, file import/export, corrupted config recovery, multiple launches, multi-monitor bounds, 125/150/200% DPI, normal close while each task type runs, and Windows logout while idle. Run a one-hour continuous click job against a dedicated blank local test window, then compare working set, private bytes, handle count, and thread count before and after.

- [ ] **Step 6: Inspect repository cleanliness and commit final docs/fixes**

Run:

```powershell
git diff --check
git status --short
git log --oneline --decorate -10
```

Commit the release-ready state:

```powershell
git add README.md LICENSES.md docs CMakeLists.txt src tests resources third_party
git commit -m "docs: prepare ClickFlow v1 release"
```
