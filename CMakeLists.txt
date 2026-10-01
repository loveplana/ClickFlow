cmake_minimum_required(VERSION 3.24)

project(ClickFlow VERSION 0.1.1 LANGUAGES C RC)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

add_library(clickflow_core STATIC
    src/core/action.c
    src/core/hotkey.c
    src/core/clicker.c
    src/core/config.c
    src/core/playback.c
    src/core/recording.c
    src/storage/json_store.c
    src/runtime/controller_win32.c
    src/platform/win32_input.c
    src/platform/win32_hotkey.c
    src/platform/win32_recorder.c
    src/platform/win32_paths.c
    src/platform/win32_single_instance.c
)
target_include_directories(clickflow_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
clickflow_enable_warnings(clickflow_core)

add_library(cjson STATIC third_party/cjson/cJSON.c)
target_include_directories(cjson PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cjson")
target_compile_definitions(cjson PRIVATE CJSON_HIDE_SYMBOLS)
target_link_libraries(clickflow_core PUBLIC cjson)

add_executable(clickflow WIN32
    src/main.c
    src/ui/app.c
    src/ui/page_clicker.c
    src/ui/page_recording.c
    src/ui/page_macro.c
    src/ui/page_settings.c
    src/ui/theme.c
    src/ui/widgets.c
    resources/clickflow.rc
)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # Nuklear's D3D11 backend stores HRESULT values only for assertions;
    # NDEBUG removes those assertions in Release builds.
    set_source_files_properties(src/ui/app.c PROPERTIES
        COMPILE_OPTIONS "-Wno-unused-but-set-variable"
    )
endif()
target_include_directories(clickflow PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nuklear"
)
target_compile_definitions(clickflow PRIVATE
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
target_link_libraries(clickflow PRIVATE
    clickflow_core
    cjson
    user32
    shell32
    ole32
    d3d11
    dxgi
    d3dcompiler
    comdlg32
)
clickflow_enable_warnings(clickflow)
if(MINGW)
    target_link_options(clickflow PRIVATE -static -static-libgcc)
endif()

add_executable(clickflow_click_counter WIN32 tools/click_counter.c resources/click_counter.rc)
target_compile_definitions(clickflow_click_counter PRIVATE
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
target_link_libraries(clickflow_click_counter PRIVATE user32 gdi32)
if(MINGW)
    target_link_options(clickflow_click_counter PRIVATE -static -static-libgcc)
endif()
clickflow_enable_warnings(clickflow_click_counter)

include(CTest)
if(BUILD_TESTING)
    add_executable(clickflow_tests
        tests/test_main.c
        tests/test_action.c
        tests/test_hotkey.c
        tests/test_clicker.c
        tests/test_storage.c
        tests/test_playback.c
        tests/test_recording.c
        tests/test_controller.c
    )
    target_link_libraries(clickflow_tests PRIVATE clickflow_core cjson)
    if(MINGW)
        target_link_options(clickflow_tests PRIVATE -static -static-libgcc)
    endif()
    clickflow_enable_warnings(clickflow_tests)
    add_test(NAME clickflow_tests COMMAND clickflow_tests)
endif()
