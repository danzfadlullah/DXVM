#pragma once

#include <string>
#include <vector>

namespace dxvmt {

  /**
   * \brief Names of the BCn scenes the harness registers
   *
   * Seven base formats plus the sRGB variants DXVM's remap path has to keep
   * distinct, matching the formats in the DXVM format policy module.
   */
  std::vector<std::string> bcnSceneNames();

  /** \brief True when this scene name exercises the BCn remap path */
  bool bcnNeedsRemap(const std::string& sceneName);

}
