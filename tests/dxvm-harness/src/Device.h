#pragma once

#include "Scene.h"

#include <string>

namespace dxvmt {

  /**
   * \brief The device line DXML prints before a suite's results
   *
   * \param featureLevel Raw D3D_FEATURE_LEVEL value (0xb000 for 11_0). Passing
   *   0 means no device was created and yields a failure line, so a broken
   *   device can never be reported as a successful creation.
   */
  std::string formatDeviceLine(unsigned featureLevel);

  /** \brief D3D9 prints no feature level: "device created" or a failure line */
  std::string formatDeviceLine9();
  std::string formatDeviceLine9Failed();

  /** \brief D3D10 prints its level textually ("10_1"), not in hex */
  std::string formatDeviceLine10(unsigned featureLevel);
  std::string formatDeviceLine10Failed();

#ifdef _WIN32

  struct D3D11Context {
    void*        device    = nullptr; // ID3D11Device*
    void*        context   = nullptr; // ID3D11DeviceContext*
    void*        swapchain = nullptr; // IDXGISwapChain*
    unsigned     featureLevel = 0u;
  };

  /**
   * \brief Create a D3D11 device on the hardware adapter
   *
   * \returns true on success; on failure \c error holds the HRESULT so the
   *   caller can report `SKIP`/`FAIL` instead of a false pass.
   */
  bool createD3D11(D3D11Context& out, std::string& error);

  /** \brief Release a context created by createD3D11 */
  void destroyD3D11(D3D11Context& ctx);

  /** \brief Adapter description for the report header */
  std::string describeGpu();

#endif

}