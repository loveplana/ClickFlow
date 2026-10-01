#ifndef CLICKFLOW_JSON_STORE_H
#define CLICKFLOW_JSON_STORE_H

#include "core/action.h"
#include "core/config.h"

#include <stddef.h>
#include <stdint.h>

#define CF_JSON_SCHEMA_VERSION 1
#define CF_JSON_MAX_FILE_BYTES (64ULL * 1024ULL * 1024ULL)

typedef struct CfJsonLimits {
    uint64_t max_file_bytes;
    size_t max_actions;
} CfJsonLimits;

CfResult cf_json_load_config(const wchar_t *path, CfConfig *out,
                             char *error, size_t error_size);
CfResult cf_json_save_config_atomic(const wchar_t *path,
                                    const CfConfig *config,
                                    char *error, size_t error_size);
CfResult cf_json_load_macro(const wchar_t *path, const char *expected_kind,
                            CfMacro *out, char *error, size_t error_size);
CfResult cf_json_load_macro_limited(const wchar_t *path,
                                    const char *expected_kind,
                                    const CfJsonLimits *limits,
                                    CfMacro *out,
                                    char *error, size_t error_size);
CfResult cf_json_save_macro_atomic(const wchar_t *path, const char *kind,
                                   const CfMacro *macro,
                                   char *error, size_t error_size);

#endif
