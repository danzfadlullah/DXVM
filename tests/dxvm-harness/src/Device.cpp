#include "Device.h"

#include <cstdio>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <d3d11.h>
  #include <dxgi.h>

  #include <cstdio>
#endif

namespace dxvmt {

  std::string formatDeviceLine(unsigned featureLevel) {
    char buf[64];

    std::snprintf(buf, sizeof(buf),
      featureLevel
        ? "device created, feature level 0x%04x"
        : "device creation failed, feature level 0x%04x",
      featureLevel);

    return buf;
  }

#ifdef _WIN32

  namespace {

    std::string formatHresult(long hr) {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "0x%08lx", static_cast<unsigned long>(hr));
      return buf;
    }

  }

  bool createD3D11(D3D11Context& out, std::string& error) {
    static const D3D_FEATURE_LEVEL levels[] = {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
    };

    ID3D11Device*        device  = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL    level   = D3D_FEATURE_LEVEL_10_0;

    const HRESULT hr = D3D11CreateDevice(
      nullptr,
      D3D_DRIVER_TYPE_HARDWARE,
      nullptr,
      D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      levels,
      static_cast<UINT>(sizeof(levels) / sizeof(levels[0])),
      D3D11_SDK_VERSION,
      &device,
      &level,
      &context);

    if (FAILED(hr)) {
      error = "D3D11CreateDevice failed " + formatHresult(hr);
      return false;
    }

    out.device       = device;
    out.context      = context;
    out.featureLevel = static_cast<unsigned>(level);
    return true;
  }

  void destroyD3D11(D3D11Context& ctx) {
    if (ctx.context)
      static_cast<ID3D11DeviceContext*>(ctx.context)->Release();
    if (ctx.device)
      static_cast<ID3D11Device*>(ctx.device)->Release();
    if (ctx.swapchain)
      static_cast<IDXGISwapChain*>(ctx.swapchain)->Release();

    ctx = D3D11Context{};
  }

  std::string describeGpu() {
    D3D11Context ctx;
    std::string error;

    if (!createD3D11(ctx, error))
      return error;

    auto* device = static_cast<ID3D11Device*>(ctx.device);
    IDXGIDevice* dxgiDevice = nullptr;
    std::string result = "unknown adapter";

    if (SUCCEEDED(device->QueryInterface(__uuidof(IDXGIDevice),
          reinterpret_cast<void**>(&dxgiDevice)))) {
      IDXGIAdapter* adapter = nullptr;

      if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
        DXGI_ADAPTER_DESC desc = { };

        if (SUCCEEDED(adapter->GetDesc(&desc))) {
          char name[256] = { };
          WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1,
            name, sizeof(name) - 1u, nullptr, nullptr);

          char buf[64];
          std::snprintf(buf, sizeof(buf), ", driver %04lx:%04lx",
            static_cast<unsigned long>(desc.VendorId),
            static_cast<unsigned long>(desc.DeviceId));

          result = std::string(name) + buf;
        }

        adapter->Release();
      }

      dxgiDevice->Release();
    }

    destroyD3D11(ctx);
    return result;
  }

#endif

}