#pragma once

#include <vulkan/vulkan.h>

namespace dxvk {

  /**
   * rief Clamp graphics pipeline state to what Mali GPUs support.
   *
   * Works on local copies of rasterization and color-blend state; the caller
   * repoints the pipeline create info at the returned copies.
   */
  struct DxvmMaliEmulatedState {
    VkPipelineRasterizationStateCreateInfo  rsInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    VkPipelineColorBlendStateCreateInfo     cbInfo = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    VkPipelineColorBlendAttachmentState     attachments[8] = {};
    uint32_t                                viewportCount = 1u;
    bool                                    hasRaster = false;
    bool                                    hasBlend = false;
  };

  void dxvmMaliEmulateGraphicsState(
          bool                              isMali,
    const VkGraphicsPipelineCreateInfo&     pipelineInfo,
          DxvmMaliEmulatedState&            out);

}
