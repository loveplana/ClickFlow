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
