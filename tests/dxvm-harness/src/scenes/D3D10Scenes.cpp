// Direct3D 10 suite.
//
// mingw-w64 ships no libd3d10core.a, so d3d10core.dll is loaded at runtime.
// That is also what lets the harness choose which DLL to exercise: the copy
// beside the executable (DXVM) wins over anything else on the search path.

#ifdef _WIN32

#include "../Device.h"
#include "../Registry.h"
#include "../Report.h"

#ifndef NOMINMAX
  #define NOMINMAX
#endif
#include <windows.h>
#include <d3d10.h>
#include <d3d10_1.h>

#include <cstdio>
#include <string>

namespace dxvmt {

  namespace {

    using D3D10CreateDevice1Fn = HRESULT (WINAPI*)(IDXGIAdapter*, D3D10_DRIVER_TYPE,
      HMODULE, UINT, D3D10_FEATURE_LEVEL1, UINT, ID3D10Device1**);

    struct Entry {
      HMODULE               module = nullptr;
      D3D10CreateDevice1Fn  create = nullptr;

      ~Entry() {
        if (module)
          FreeLibrary(module);
      }
    };

    Entry& entry() {
      static Entry e;
      static bool tried = false;

      if (!tried) {
        tried = true;
        // d3d10core.dll is the DLL DXVM ships and the one apps load.
        e.module = LoadLibraryA("d3d10core.dll");

        if (e.module) {
          e.create = reinterpret_cast<D3D10CreateDevice1Fn>(
            reinterpret_cast<void*>(GetProcAddress(e.module, "D3D10CreateDevice1")));
        }
      }

      return e;
    }

    SceneResult createAndReport(unsigned wantedLevel, const char* levelName) {
      Entry& e = entry();

      if (!e.module)
        return SceneResult::skip("d3d10core.dll not loadable");

      if (!e.create)
        return SceneResult::skip("D3D10CreateDevice1 not exported by d3d10core.dll");

      ID3D10Device1* device = nullptr;

      const HRESULT hr = e.create(nullptr, D3D10_DRIVER_TYPE_HARDWARE, nullptr, 0u,
        static_cast<D3D10_FEATURE_LEVEL1>(wantedLevel), D3D10_SDK_VERSION, &device);

      if (FAILED(hr) || !device) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "D3D10CreateDevice1 failed 0x%08lx",
          static_cast<unsigned long>(hr));
        return SceneResult::fail(buf);
      }

      const D3D10_FEATURE_LEVEL1 actual = device->GetFeatureLevel();
      device->Release();

      if (static_cast<unsigned>(actual) < wantedLevel) {
        return SceneResult::fail(std::string("got level ") + std::to_string(static_cast<int>(actual))
          + ", wanted at least " + levelName);
      }

      return SceneResult::pass(std::string("feature level ") + levelName);
    }

    SceneResult sceneD3D10Create() {
      return createAndReport(static_cast<unsigned>(D3D10_FEATURE_LEVEL_10_0), "10_0");
    }

    SceneResult sceneD3D10Level101() {
      return createAndReport(static_cast<unsigned>(D3D10_FEATURE_LEVEL_10_1), "10_1");
    }

    SceneResult notPorted() {
      return SceneResult::skip("not ported yet");
    }

  }

  void registerD3D10Scenes(Registry& registry) {
    registry.add("d3d10.create",      &sceneD3D10Create);
    registry.add("d3d10.level101",    &sceneD3D10Level101);
    registry.add("d3d10.rendertarget",&notPorted);
    registry.add("d3d10.texture",     &notPorted);
    registry.add("d3d10.blend",       &notPorted);
    registry.add("d3d10.depth",       &notPorted);
  }

}

#endif