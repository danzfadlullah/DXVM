#include "../dxvk_include.h"
#include "dxvm_mali_hints.h"
#include "../dxvk_device.h"
#include "../dxvk_shader.h"

namespace dxvk {

void dxvmMaliApplyHints(
          const DxvmMaliConfig&   config,
          VkDriverId              driverId,
          bool                    isValhall,
          DxvkDevicePerfHints&    hints) {
    uint32_t f = dxvmMaliHintFlags(config, driverId, isValhall);
    if (f & DXVM_HINT_DYNAMIC_RENDERING) hints.preferDynamicRendering = VK_TRUE;
    if (f & DXVM_HINT_COMPUTE_MIPGEN) hints.preferComputeMipGen = VK_TRUE;
    if (f & DXVM_HINT_DESCRIPTOR_OFFSETS) hints.preferDescriptorByteOffsets = VK_TRUE;
    if (f & DXVM_HINT_CACHED_MEMORY) hints.preferCachedMemory = VK_TRUE;
    if (f & DXVM_HINT_RENDERPASS_OPS) hints.preferRenderPassOps = VK_TRUE;
  }

  void dxvmMaliApplyShaderOptions(
          const DxvmMaliConfig&   config,
          bool                    isMali,
          bool                    hasFloat16,
          bool                    hasInt8,
          DxvkShaderOptions&      options) {
    if (dxvmMaliUse16Bit(config, isMali, hasFloat16, hasInt8))
      options.flags.set(DxvkShaderCompileFlag::Supports16BitArithmetic);
  }

}
