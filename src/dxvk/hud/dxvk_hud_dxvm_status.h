#pragma once

#include <string>

namespace dxvk::hud {

  /**
   * \brief Build the DXVM HUD status line for the C1 compact panel.
   *
   * \param useCpuBcn  True when the CPU BCn fallback is active
   * \param dynRender  True when dynamic rendering is enabled
   * \param sync2      True when synchronization2 is enabled
   */
  std::string dxvmHudStatusLine(bool useCpuBcn, bool dynRender, bool sync2);

}
