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
