#pragma once

// Minimal host-side shim for DXVK's unconditional windows.h include.
// Provides only the typedefs needed to parse Vulkan/vulkan_win32.h and
// DXVK headers on a non-Windows host. No behavior — types only.
typedef wchar_t WCHAR;
typedef const WCHAR* LPCWSTR;
typedef WCHAR* LPWSTR;
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* HMONITOR;
typedef unsigned long DWORD;
typedef void* SECURITY_ATTRIBUTES;
typedef void* HINSTANCE;
typedef void* HWND;
typedef void* HDC;
typedef long HRESULT;
typedef unsigned long long UINT64;
typedef long long INT64;
typedef unsigned int UINT;
typedef void* D3DKMT_HANDLE;
typedef struct { unsigned long LowPart; long HighPart; } LUID;
typedef unsigned long NTSTATUS;
typedef unsigned long ULONG;
#define WINAPI
#define WINBASEAPI
#define EXTERN_C extern "C"
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((void*)-1)
#endif
typedef void* OBJECT_ATTRIBUTES;
