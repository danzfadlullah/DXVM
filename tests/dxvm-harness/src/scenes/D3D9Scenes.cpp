// Direct3D 9 suite.
//
// Real device creation plus the two checks the reference report shows for
// D3D9: a working device and MSAA quality query.

#ifdef _WIN32

#include "../Device.h"
#include "../Registry.h"
#include "../Report.h"

#ifndef NOMINMAX
  #define NOMINMAX
#endif
#include <windows.h>
#include <d3d9.h>

#include <cstdio>
#include <string>

namespace dxvmt {

  namespace {

    struct D3D9Context {
      IDirect3D9*       d3d    = nullptr;
      IDirect3DDevice9* device = nullptr;
      HWND              hwnd   = nullptr;

      ~D3D9Context() {
        if (device) device->Release();
        if (d3d)    d3d->Release();
        if (hwnd)   DestroyWindow(hwnd);
      }
    };

    bool createD3D9(D3D9Context& out, std::string& error) {
      out.d3d = Direct3DCreate9(D3D_SDK_VERSION);

      if (!out.d3d) {
        error = "Direct3DCreate9 failed";
        return false;
      }

      WNDCLASSA wc = { };
      wc.lpfnWndProc   = DefWindowProcA;
      wc.hInstance     = GetModuleHandleA(nullptr);
      wc.lpszClassName = "DxvmHarnessD3D9";
      RegisterClassA(&wc);

      out.hwnd = CreateWindowExA(0, wc.lpszClassName, "dxvm-harness",
        WS_POPUP, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);

      if (!out.hwnd) {
        error = "CreateWindowEx failed";
        return false;
      }

      D3DPRESENT_PARAMETERS pp = { };
      pp.Windowed         = TRUE;
      pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
      pp.BackBufferFormat = D3DFMT_X8R8G8B8;
      pp.BackBufferWidth  = 64u;
      pp.BackBufferHeight = 64u;

      const HRESULT hr = out.d3d->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, out.hwnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &out.device);

      if (FAILED(hr)) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "CreateDevice failed 0x%08lx",
          static_cast<unsigned long>(hr));
        error = buf;
        return false;
      }

      return true;
    }

    SceneResult sceneD3D9Device() {
      D3D9Context ctx;
      std::string error;

      if (!createD3D9(ctx, error))
        return SceneResult::fail(error);

      D3DCAPS9 caps = { };
      ctx.device->GetDeviceCaps(&caps);

      char buf[96];
      std::snprintf(buf, sizeof(buf), "%ux%u, vertex shader %u.%u",
        caps.MaxTextureWidth, caps.MaxTextureHeight,
        caps.VertexShaderVersion >> 8, caps.VertexShaderVersion & 0xFFu);

      return SceneResult::pass(buf);
    }

    SceneResult sceneD3D9Msaa() {
      D3D9Context ctx;
      std::string error;

      if (!createD3D9(ctx, error))
        return SceneResult::fail(error);

      std::string detail;
      std::string unsupported;

      for (D3DMULTISAMPLE_TYPE type : { D3DMULTISAMPLE_2_SAMPLES,
                                        D3DMULTISAMPLE_4_SAMPLES,
                                        D3DMULTISAMPLE_8_SAMPLES }) {
        DWORD quality = 0u;
        const HRESULT hr = ctx.d3d->CheckDeviceMultiSampleType(
          D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
          TRUE, type, &quality);

        const bool ok = SUCCEEDED(hr) && quality > 0u;

        if (!detail.empty()) detail += ", ";
        detail += std::to_string(static_cast<int>(type)) + "x=" + (ok ? "yes" : "no");

        if (!ok) {
          if (!unsupported.empty()) unsupported += ", ";
          unsupported += std::to_string(static_cast<int>(type)) + "x";
        }
      }

      // DXML's D3D9 MSAA check reports the counts the driver offers; a device
      // offering none is a finding, not a pass.
      if (!unsupported.empty() && detail.find("yes") == std::string::npos)
        return SceneResult::fail("no multisample type supported: " + detail);

      return SceneResult::pass("multisample types: " + detail);
    }

  }

  void registerD3D9Scenes(Registry& registry) {
    registry.add("d3d9.device", &sceneD3D9Device);
    registry.add("d3d9.msaa",   &sceneD3D9Msaa);
  }

}

#endif