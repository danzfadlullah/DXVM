// Direct3D 11 device and capability scenes.
//
// Windows-only: this translation unit is compiled out on the Linux build host,
// where only the pure report helpers are unit-tested.

#ifdef _WIN32

#include "../Device.h"
#include "../Registry.h"
#include "../Report.h"

#ifndef NOMINMAX
  #define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>

#include <cstdio>
#include <string>

namespace dxvmt {

  namespace {

    std::string hex32(unsigned long long v) {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "0x%08llx", v);
      return buf;
    }

    // device.create — the device exists and reports a sane feature level.
    SceneResult sceneDeviceCreate() {
      D3D11Context ctx;
      std::string error;

      if (!createD3D11(ctx, error))
        return SceneResult::fail(error);

      const std::string detail = formatDeviceLine(ctx.featureLevel);
      destroyD3D11(ctx);

      // formatDeviceLine already carries the "device created, ..." wording;
      // reuse its tail so the report line reads naturally.
      const std::string tail = detail.substr(std::string("device created, ").size());
      return SceneResult::pass(tail);
    }

    // device.featurelevels — at least 11_0, which is what DXML's suite needs.
    SceneResult sceneDeviceFeatureLevels() {
      D3D11Context ctx;
      std::string error;

      if (!createD3D11(ctx, error))
        return SceneResult::fail(error);

      const unsigned level = ctx.featureLevel;
      destroyD3D11(ctx);

      if (level < 0xb000u) {
        return SceneResult::fail("feature level " + hex32(level)
          + ", expected at least " + hex32(0xb000u));
      }

      return SceneResult::pass("feature level " + hex32(level));
    }

    // device.formats — formats D3D11 requires for render targets and SRVs.
    SceneResult sceneDeviceFormats() {
      D3D11Context ctx;
      std::string error;

      if (!createD3D11(ctx, error))
        return SceneResult::fail(error);

      auto* device = static_cast<ID3D11Device*>(ctx.device);

      static const struct { DXGI_FORMAT format; const char* name; } required[] = {
        { DXGI_FORMAT_R8G8B8A8_UNORM,     "R8G8B8A8_UNORM"     },
        { DXGI_FORMAT_B8G8R8A8_UNORM,     "B8G8R8A8_UNORM"     },
        { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,"R8G8B8A8_UNORM_SRGB"},
        { DXGI_FORMAT_R32G32B32A32_FLOAT, "R32G32B32A32_FLOAT" },
        { DXGI_FORMAT_R16G16B16A16_FLOAT, "R16G16B16A16_FLOAT" },
        { DXGI_FORMAT_R32_FLOAT,          "R32_FLOAT"          },
        { DXGI_FORMAT_D32_FLOAT,          "D32_FLOAT"          },
        { DXGI_FORMAT_D24_UNORM_S8_UINT,  "D24_UNORM_S8_UINT"  },
      };

      unsigned checked = 0u;
      std::string missing;

      for (const auto& f : required) {
        UINT support = 0u;
        const HRESULT hr = device->CheckFormatSupport(f.format, &support);
        checked++;

        if (FAILED(hr) || support == 0u) {
          if (!missing.empty())
            missing += ", ";
          missing += f.name;
        }
      }

      destroyD3D11(ctx);

      if (!missing.empty())
        return SceneResult::fail("no support for " + missing);

      return SceneResult::pass(std::to_string(checked) + " of "
        + std::to_string(checked) + " formats supported");
    }

  }

  void registerDeviceScenes(Registry& registry) {
    registry.add("device.create",        &sceneDeviceCreate);
    registry.add("device.featurelevels", &sceneDeviceFeatureLevels);
    registry.add("device.formats",       &sceneDeviceFormats);
  }

}

#endif