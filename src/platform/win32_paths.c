#include "platform/win32_paths.h"

#include <windows.h>
#include <shlobj.h>

#include <wchar.h>

static CfResult append_component(wchar_t *buffer, size_t buffer_count,
                                 const wchar_t *component)
{
    size_t used = wcslen(buffer);
    size_t added = wcslen(component);

    if (used + added + 1U > buffer_count) {
        return CF_ERR_LIMIT;
    }
    memcpy(buffer + used, component, (added + 1U) * sizeof(*buffer));
    return CF_OK;
}

CfResult cf_win32_data_directory(wchar_t *buffer, size_t buffer_count)
{
    PWSTR local = NULL;
    size_t length;
    HRESULT result;

    if (buffer == NULL || buffer_count == 0) {
        return CF_ERR_INVALID_ARGUMENT;
    }
    result = SHGetKnownFolderPath(&FOLDERID_LocalAppData, KF_FLAG_DEFAULT,
                                  NULL, &local);
    if (FAILED(result) || local == NULL) {
        return CF_ERR_PLATFORM;
    }
    length = wcslen(local);
    if (length + wcslen(L"\\ClickFlow") + 1U > buffer_count) {
        CoTaskMemFree(local);
        return CF_ERR_LIMIT;
    }
    wcscpy(buffer, local);
    CoTaskMemFree(local);
    return append_component(buffer, buffer_count, L"\\ClickFlow");
}

static CfResult ensure_directory(const wchar_t *path)
{
    if (CreateDirectoryW(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return CF_OK;
    }
    return CF_ERR_IO;
}

CfResult cf_win32_ensure_data_directories(void)
{
    wchar_t path[MAX_PATH];
    size_t base_length;
    CfResult result = cf_win32_data_directory(path, CF_ARRAY_COUNT(path));

    if (result != CF_OK) return result;
    result = ensure_directory(path);
    if (result != CF_OK) return result;
    base_length = wcslen(path);
    result = append_component(path, CF_ARRAY_COUNT(path), L"\\logs");
    if (result == CF_OK) result = ensure_directory(path);
    path[base_length] = L'\0';
    if (result == CF_OK) {
        result = append_component(path, CF_ARRAY_COUNT(path), L"\\backups");
    }
    if (result == CF_OK) result = ensure_directory(path);
    return result;
}
