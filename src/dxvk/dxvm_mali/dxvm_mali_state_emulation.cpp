#include "dxvm_mali_state_emulation.h"
#include <cstring>

namespace dxvk {

  static bool isDualSourceFactor(VkBlendFactor f) {
    return f == VK_BLEND_FACTOR_SRC1_COLOR
        || f == VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR
        || f == VK_BLEND_FACTOR_SRC1_ALPHA
        || f == VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
  }

  static VkBlendFactor clampFactor(VkBlendFactor f) {
    switch (f) {
      case VK_BLEND_FACTOR_SRC1_COLOR: return VK_BLEND_FACTOR_SRC_COLOR;
      case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
      case VK_BLEND_FACTOR_SRC1_ALPHA: return VK_BLEND_FACTOR_SRC_ALPHA;
      case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      default: return f;
    }
  }

  void dxvmMaliEmulateGraphicsState(
          bool                              isMali,
    const VkGraphicsPipelineCreateInfo&     pipelineInfo,
          DxvmMaliEmulatedState&            out) {
    if (!isMali)
      return;

    if (pipelineInfo.pRasterizationState) {
      out.rsInfo = *pipelineInfo.pRasterizationState;
      if (out.rsInfo.polygonMode != VK_POLYGON_MODE_FILL)
        out.rsInfo.polygonMode = VK_POLYGON_MODE_FILL;
      out.hasRaster = true;
    }

    if (pipelineInfo.pColorBlendState) {
      out.cbInfo = *pipelineInfo.pColorBlendState;
      out.cbInfo.logicOpEnable = VK_FALSE;
      uint32_t n = out.cbInfo.attachmentCount;
      if (n > 8u) n = 8u;
      if (out.cbInfo.pAttachments && n) {
        std::memcpy(out.attachments, out.cbInfo.pAttachments,
                    n * sizeof(VkPipelineColorBlendAttachmentState));
        for (uint32_t i = 0; i < n; i++) {
          auto& a = out.attachments[i];
          if (isDualSourceFactor(a.srcColorBlendFactor)) a.srcColorBlendFactor = clampFactor(a.srcColorBlendFactor);
          if (isDualSourceFactor(a.dstColorBlendFactor)) a.dstColorBlendFactor = clampFactor(a.dstColorBlendFactor);
          if (isDualSourceFactor(a.srcAlphaBlendFactor)) a.srcAlphaBlendFactor = clampFactor(a.srcAlphaBlendFactor);
          if (isDualSourceFactor(a.dstAlphaBlendFactor)) a.dstAlphaBlendFactor = clampFactor(a.dstAlphaBlendFactor);
        }
        out.cbInfo.pAttachments = out.attachments;
      }
      out.hasBlend = true;
    }

    uint32_t requested = pipelineInfo.pViewportState ? pipelineInfo.pViewportState->viewportCount : 1u;
    out.viewportCount = requested > 1u ? 1u : requested;
  }

}
