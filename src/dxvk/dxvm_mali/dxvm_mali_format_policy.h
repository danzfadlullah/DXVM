#pragma once

#include <vulkan/vulkan.h>

namespace dxvk {

  /**
   * \brief Remap a BCn format to what the device can actually create.
   *
   * - Non-Mali: identity (upstream behavior).
   * - Mali + GPU dispatch: BCn -> ASTC (sRGB preserved).
   * - Mali + CPU fallback: BCn -> RGBA8 (sRGB preserved), so the CPU
   *   transcoder output matches the image format exactly.
   */
  VkFormat dxvmMaliRemapFormat(bool isMali, bool useCpuBcn, VkFormat format);

  /** \brief True if the given format is a BCn block-compressed format. */
  bool dxvmIsBcnFormat(VkFormat format);

}
