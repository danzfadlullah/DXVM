#pragma once

#include "dxvm_mali_config.h"
#include <vulkan/vulkan.h>

namespace dxvk {

  struct DxvkDevicePerfHints;
  struct DxvkShaderOptions;

  // Hint flags returned by the pure helper (no DXVK device types needed).
  enum DxvmMaliHintFlag : uint32_t {
    DXVM_HINT_DYNAMIC_RENDERING = 1u << 0,
    DXVM_HINT_COMPUTE_MIPGEN   = 1u << 1,
    DXVM_HINT_DESCRIPTOR_OFFSETS = 1u << 2,
    DXVM_HINT_CACHED_MEMORY    = 1u << 3,
    DXVM_HINT_RENDERPASS_OPS   = 1u << 4,
  };

  /** Pure helper: which hints apply. 0 for non-Mali. */
  uint32_t dxvmMaliHintFlags(const DxvmMaliConfig& config, VkDriverId driverId, bool isValhall);

  /** Pure helper: whether 16-bit arithmetic should be enabled. */
  bool dxvmMaliUse16Bit(const DxvmMaliConfig& config, bool isMali, bool hasFloat16, bool hasInt8);

  void dxvmMaliApplyHints(const DxvmMaliConfig& config, VkDriverId driverId,
                          bool isValhall, DxvkDevicePerfHints& hints);

  void dxvmMaliApplyShaderOptions(const DxvmMaliConfig& config, bool isMali,
                                  bool hasFloat16, bool hasInt8, DxvkShaderOptions& options);

}
