#include "core/cf_result.h"
#include "test.h"

#include <string.h>

int cf_test_failures = 0;

void cf_test_action(void);
void cf_test_clicker(void);
void cf_test_hotkey(void);
void cf_test_storage(void);
void cf_test_playback(void);
void cf_test_recording(void);
void cf_test_controller(void);

static int selected(const char *name, int argc, char **argv)
{
    int index;

    if (argc == 1) {
        return 1;
    }
    for (index = 1; index < argc; ++index) {
        if (strcmp(name, argv[index]) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    CF_TEST_ASSERT_EQ(CF_OK, 0);
    if (selected("action", argc, argv)) {
        cf_test_action();
    }
    if (selected("hotkey", argc, argv)) {
        cf_test_hotkey();
    }
    if (selected("clicker", argc, argv)) {
        cf_test_clicker();
    }
    if (selected("storage", argc, argv)) {
        cf_test_storage();
    }
    if (selected("playback", argc, argv)) {
        cf_test_playback();
    }
    if (selected("recording", argc, argv)) {
        cf_test_recording();
    }
    if (selected("controller", argc, argv)) {
        cf_test_controller();
    }

    if (cf_test_failures != 0) {
        fprintf(stderr, "%d test assertion(s) failed\n", cf_test_failures);
        return 1;
    }

    puts("all tests passed");
    return 0;
}
