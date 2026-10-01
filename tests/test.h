#ifndef CLICKFLOW_TEST_H
#define CLICKFLOW_TEST_H

#include <stdio.h>

extern int cf_test_failures;

#define CF_TEST_ASSERT(condition)                                                \
    do {                                                                         \
        if (!(condition)) {                                                       \
            fprintf(stderr, "%s:%d: assertion failed: %s\n",                   \
                    __FILE__, __LINE__, #condition);                              \
            ++cf_test_failures;                                                   \
        }                                                                         \
    } while (0)

#define CF_TEST_ASSERT_EQ(actual, expected)                                      \
    do {                                                                         \
        long long cf_actual_value = (long long)(actual);                          \
        long long cf_expected_value = (long long)(expected);                      \
        if (cf_actual_value != cf_expected_value) {                               \
            fprintf(stderr, "%s:%d: expected %lld, got %lld\n",                \
                    __FILE__, __LINE__, cf_expected_value, cf_actual_value);       \
            ++cf_test_failures;                                                   \
        }                                                                         \
    } while (0)

#endif
