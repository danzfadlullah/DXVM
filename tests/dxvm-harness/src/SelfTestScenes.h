#pragma once

#include "Registry.h"

namespace dxvmt {

  /**
   * \brief Register the built-in stub scenes
   *
   * These need no Direct3D device, so the report pipeline and crash
   * containment are verifiable on a build host without Wine.
   */
  void registerSelfTestScenes(Registry& registry);

}