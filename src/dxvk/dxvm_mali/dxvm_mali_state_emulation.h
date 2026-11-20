#pragma once

#include <vulkan/vulkan.h>

namespace dxvk {

  /**
   * \brief Clamp graphics pipeline state to what Mali GPUs support.
   *
   * Non-Mali devices are untouched. On Mali:
   *  - polygon mode LINE/POINT -> FILL
   *  - dual-source blend factors -> ordinary blend factors
   *  - viewport/scissor count -> 1
   *  - logic op -> disabled
   */
  void dxvmMaliEmulateGraphicsState(
          bool                              isMali,
          VkGraphicsPipelineCreateInfo&     pipelineInfo,
          VkPipelineColorBlendAttachmentState* attachments,
          uint32_t&                         attachmentCount,
          uint32_t&                         viewportCount);

}
