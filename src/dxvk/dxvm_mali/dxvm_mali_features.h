#pragma once

#include "../dxvk_device_info.h"

namespace dxvk {

  /**
   * \brief Apply Mali-specific feature overrides
   *
   * On non-Mali devices this is a strict no-op, preserving upstream DXVK
   * feature enablement byte-for-byte. On Mali devices it disables core
   * features the GPU lacks and enables optional ARM/KHR features whose
   * extensions are reported as supported.
   */
  void dxvmMaliChainFeatures(
          bool                    isMali,
          DxvkDeviceExtensionInfo&  extensions,
          DxvkDeviceFeatures&       features);

}
