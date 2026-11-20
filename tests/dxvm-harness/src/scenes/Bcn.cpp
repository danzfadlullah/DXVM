#include "Bcn.h"

namespace dxvmt {

  std::vector<std::string> bcnSceneNames() {
    return {
      // Base formats
      "bcn.bc1", "bcn.bc2", "bcn.bc3", "bcn.bc4", "bcn.bc5", "bcn.bc6h", "bcn.bc7",
      // sRGB variants that must stay sRGB after the remap
      "bcn.bc1_srgb", "bcn.bc2_srgb", "bcn.bc3_srgb", "bcn.bc6h_srgb", "bcn.bc7_srgb",
    };
  }

  bool bcnNeedsRemap(const std::string& sceneName) {
    const auto names = bcnSceneNames();
    for (const auto& n : names) {
      if (n == sceneName)
        return true;
    }
    return false;
  }

}
