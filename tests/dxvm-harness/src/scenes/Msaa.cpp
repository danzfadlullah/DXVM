#include "Msaa.h"

namespace dxvmt {

  // DXGI_FORMAT numeric values, kept as plain ints so this file builds on a
  // host without the Direct3D headers.
  namespace {
    constexpr int kRgba8   = 28;  // DXGI_FORMAT_R8G8B8A8_UNORM
    constexpr int kRgba16f = 10;  // DXGI_FORMAT_R16G16B16A16_FLOAT
    constexpr int kBgra8   = 87;  // DXGI_FORMAT_B8G8R8A8_UNORM
  }

  int expectedSampleIndexRamp(int sampleCount) {
    switch (sampleCount) {
      case 2:  return 9;    // DXML passes; stock DXVK 3.1.1 reports 26
      case 4:  return 26;   // passes in both
      case 8:  return 60;
      case 16: return 128;  // DXML passes; stock DXVK 3.1.1 reports 26
      default: return -1;
    }
  }

  std::vector<MsaaFormat> msaaFormats() {
    return {
      MsaaFormat{ "rgba8",   kRgba8   },
      MsaaFormat{ "rgba16f", kRgba16f },
      MsaaFormat{ "bgra8",   kBgra8   },
    };
  }

  int msaaPartialPixelExpectation() {
    return 64;
  }

  std::pair<int, int> msaaPositionExpectation() {
    return { 42, 82 };
  }

  std::string msaaTexture2dmsExpectation() {
    return "128,64,255";
  }

}