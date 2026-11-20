#pragma once

#include <vulkan/vulkan.h>

namespace dxvk {

  /**
   * \brief Remap a BCn format to what the device can actually create.
   *
   * - Non-Mali: identity (upstream behavior).
   * - Mali with transcoding disabled: identity (respects dxvk.enableBcnTranscoding).
   * - Mali + GPU dispatch: BCn -> ASTC (sRGB preserved).
   * - Mali + CPU fallback: BCn -> RGBA8 (sRGB preserved), so the CPU
   *   transcoder output matches the image format exactly.
   */
  VkFormat dxvmMaliRemapFormat(bool isMali, bool useCpuBcn,
                               bool enableBcnTranscoding, VkFormat format);

  /**
   * \brief Remap a BCn view format to the image's actual format
   *
   * Preserves the color space of the requested BCn view when the image was
   * created from a remapped BCn format. Identity for all other formats.
   */
  VkFormat dxvmMaliRemapViewFormat(VkFormat requestedViewFormat, VkFormat imageFormat);

  /** \brief True if the given BCn format is an sRGB variant. */
  bool dxvmIsSrgbBcnFormat(VkFormat format);

  /** \brief True if the given format is a BCn block-compressed format. */
  bool dxvmIsBcnFormat(VkFormat format);

}
