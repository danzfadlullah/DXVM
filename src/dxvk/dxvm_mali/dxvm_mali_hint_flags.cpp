#include "dxvm_mali_hints.h"

namespace dxvk {

uint32_t dxvmMaliHintFlags(
          const DxvmMaliConfig&   config,
          VkDriverId              driverId,
          bool                    isValhall) {
    bool isMali = driverId == VK_DRIVER_ID_ARM_PROPRIETARY
               || driverId == VK_DRIVER_ID_MESA_PANVK;
    if (!isMali)
      return 0;

    uint32_t flags = DXVM_HINT_DESCRIPTOR_OFFSETS
                   | DXVM_HINT_CACHED_MEMORY
                   | DXVM_HINT_RENDERPASS_OPS;
    if (config.enableDynamicRendering)
      flags |= DXVM_HINT_DYNAMIC_RENDERING;
    if (isValhall)
      flags |= DXVM_HINT_COMPUTE_MIPGEN;
    return flags;
  }

  bool dxvmMaliUse16Bit(
          const DxvmMaliConfig&   config,
          bool                    isMali,
          bool                    hasFloat16,
          bool                    hasInt8) {
    if (!isMali)
      return false;
    return (hasFloat16 && (config.enableFP16Shaders || isMali))
        || (hasInt8 && (config.enableINT8Shaders || isMali));
  }

  }
