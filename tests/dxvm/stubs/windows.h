#pragma once

// Minimal host-side shim for DXVK's unconditional windows.h include.
// Only the WCHAR typedef is required by the config sources we compile.
typedef wchar_t WCHAR;
