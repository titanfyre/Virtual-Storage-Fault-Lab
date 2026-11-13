/* SPDX-License-Identifier: MIT */
#ifndef VIRTUAL_STORAGE_FAULT_LAB_CSDK_H
#define VIRTUAL_STORAGE_FAULT_LAB_CSDK_H
#include <stdint.h>
#if !defined(_WIN32) || !defined(_WIN64)
#error This SDK requires Windows x64.
#endif
#ifdef __cplusplus
#define VIRTUAL_STORAGE_FAULT_LAB_CSDK_NOEXCEPT noexcept
extern "C" {
#else
#define VIRTUAL_STORAGE_FAULT_LAB_CSDK_NOEXCEPT
#endif
#if defined(VIRTUAL_STORAGE_FAULT_LAB_CSDK_BUILD)
#define VIRTUAL_STORAGE_FAULT_LAB_CSDK_API __declspec(dllexport)
#else
#define VIRTUAL_STORAGE_FAULT_LAB_CSDK_API __declspec(dllimport)
#endif
/* open is inert. One active session per loaded product DLL.
   call accepts length-delimited UTF-8 JSON and returns an allocated UTF-8
   response, or NULL on allocation failure. Always free with this same DLL.
   Session/operation handles are scoped to the client that returned them. */
VIRTUAL_STORAGE_FAULT_LAB_CSDK_API uint32_t virtual_storage_fault_lab_json_open(uint64_t *session) VIRTUAL_STORAGE_FAULT_LAB_CSDK_NOEXCEPT;
VIRTUAL_STORAGE_FAULT_LAB_CSDK_API uint32_t virtual_storage_fault_lab_json_close(uint64_t session) VIRTUAL_STORAGE_FAULT_LAB_CSDK_NOEXCEPT;
VIRTUAL_STORAGE_FAULT_LAB_CSDK_API char *virtual_storage_fault_lab_json_call(uint64_t session,const char *request,uint32_t length) VIRTUAL_STORAGE_FAULT_LAB_CSDK_NOEXCEPT;
VIRTUAL_STORAGE_FAULT_LAB_CSDK_API void virtual_storage_fault_lab_json_free(char *response) VIRTUAL_STORAGE_FAULT_LAB_CSDK_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef VIRTUAL_STORAGE_FAULT_LAB_CSDK_NOEXCEPT
#endif
