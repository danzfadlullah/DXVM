#include "dxvm_mali_state_emulation.h"

namespace dxvk {

  static bool isDualSourceFactor(VkBlendFactor f) {
    return f == VK_BLEND_FACTOR_SRC1_COLOR
        || f == VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR
        || f == VK_BLEND_FACTOR_SRC1_ALPHA
        || f == VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
  }

  static VkBlendFactor clampFactor(VkBlendFactor f) {
    switch (f) {
      case VK_BLEND_FACTOR_SRC1_COLOR:         return VK_BLEND_FACTOR_SRC_COLOR;
      case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
      case VK_BLEND_FACTOR_SRC1_ALPHA:         return VK_BLEND_FACTOR_SRC_ALPHA;
      case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      default: return f;
    }
  }

  void dxvmMaliEmulateGraphicsState(
          bool                              isMali,
          VkGraphicsPipelineCreateInfo&     pipelineInfo,
          VkPipelineColorBlendAttachmentState* attachments,
          uint32_t&                         attachmentCount,
          uint32_t&                         viewportCount) {
    if (!isMali)
      return;

    // Wireframe / point modes unsupported -> solid fill.
    if (pipelineInfo.pRasterizationState &&
        (pipelineInfo.pRasterizationState->polygonMode == VK_POLYGON_MODE_LINE ||
         pipelineInfo.pRasterizationState->polygonMode == VK_POLYGON_MODE_POINT)) {
      // Need a mutable copy: callers pass a non-const struct with const members;
      // use const_cast on the pointed-to state, which the emulator owns in practice.
      auto* rs = const_cast<VkPipelineRasterizationStateCreateInfo*>(pipelineInfo.pRasterizationState);
      rs->polygonMode = VK_POLYGON_MODE_FILL;
    }

    // Dual-source blending unsupported -> ordinary factors.
    if (attachments) {
      for (uint32_t i = 0; i < attachmentCount; i++) {
        auto& a = attachments[i];
        if (isDualSourceFactor(a.srcColorBlendFactor)) a.srcColorBlendFactor = clampFactor(a.srcColorBlendFactor);
        if (isDualSourceFactor(a.dstColorBlendFactor)) a.dstColorBlendFactor = clampFactor(a.dstColorBlendFactor);
        if (isDualSourceFactor(a.srcAlphaBlendFactor)) a.srcAlphaBlendFactor = clampFactor(a.srcAlphaBlendFactor);
        if (isDualSourceFactor(a.dstAlphaBlendFactor)) a.dstAlphaBlendFactor = clampFactor(a.dstAlphaBlendFactor);
      }
    }

    // Single viewport only.
    if (viewportCount > 1u)
      viewportCount = 1u;

    // Logic ops unsupported: clear enable in blend state if provided.
    // pColorBlendState has logicOpEnable; use const_cast as above.
    if (pipelineInfo.pColorBlendState) {
      auto* cb = const_cast<VkPipelineColorBlendStateCreateInfo*>(pipelineInfo.pColorBlendState);
      cb->logicOpEnable = VK_FALSE;
    }
  }

}
