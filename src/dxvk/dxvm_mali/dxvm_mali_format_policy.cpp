#include "dxvm_mali_format_policy.h"

namespace dxvk {

  bool dxvmIsBcnFormat(VkFormat f) {
    switch (f) {
      case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
      case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
      case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
      case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
      case VK_FORMAT_BC2_UNORM_BLOCK:
      case VK_FORMAT_BC2_SRGB_BLOCK:
      case VK_FORMAT_BC3_UNORM_BLOCK:
      case VK_FORMAT_BC3_SRGB_BLOCK:
      case VK_FORMAT_BC4_UNORM_BLOCK:
      case VK_FORMAT_BC4_SNORM_BLOCK:
      case VK_FORMAT_BC5_UNORM_BLOCK:
      case VK_FORMAT_BC5_SNORM_BLOCK:
      case VK_FORMAT_BC6H_UFLOAT_BLOCK:
      case VK_FORMAT_BC6H_SFLOAT_BLOCK:
      case VK_FORMAT_BC7_UNORM_BLOCK:
      case VK_FORMAT_BC7_SRGB_BLOCK:
        return true;
      default:
        return false;
    }
  }

  bool dxvmIsSrgbBcnFormat(VkFormat f) {
    switch (f) {
      case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
      case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
      case VK_FORMAT_BC2_SRGB_BLOCK:
      case VK_FORMAT_BC3_SRGB_BLOCK:
      case VK_FORMAT_BC7_SRGB_BLOCK:
        return true;
      default:
        return false;
    }
  }

  VkFormat dxvmMaliRemapFormat(bool isMali, bool useCpuBcn,
                               bool enableBcnTranscoding, VkFormat format) {
    if (!isMali || !enableBcnTranscoding || !dxvmIsBcnFormat(format))
      return format;

    bool srgb = dxvmIsSrgbBcnFormat(format);

    // CPU fallback: image must be RGBA8 to match the CPU decoder output.
    if (useCpuBcn)
      return srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

    // GPU dispatch: map to ASTC with matching color space.
    switch (format) {
      case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
      case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
      case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
      case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
      case VK_FORMAT_BC2_UNORM_BLOCK:
      case VK_FORMAT_BC2_SRGB_BLOCK:
      case VK_FORMAT_BC3_UNORM_BLOCK:
      case VK_FORMAT_BC3_SRGB_BLOCK:
      case VK_FORMAT_BC4_UNORM_BLOCK:
      case VK_FORMAT_BC4_SNORM_BLOCK:
      case VK_FORMAT_BC5_UNORM_BLOCK:
      case VK_FORMAT_BC5_SNORM_BLOCK:
        return srgb ? VK_FORMAT_ASTC_4x4_SRGB_BLOCK : VK_FORMAT_ASTC_4x4_UNORM_BLOCK;

      case VK_FORMAT_BC6H_UFLOAT_BLOCK:
      case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        return VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK;

      case VK_FORMAT_BC7_UNORM_BLOCK:
      case VK_FORMAT_BC7_SRGB_BLOCK:
        return srgb ? VK_FORMAT_ASTC_6x6_SRGB_BLOCK : VK_FORMAT_ASTC_6x6_UNORM_BLOCK;

      default:
        return format;
    }
  }

  VkFormat dxvmMaliRemapViewFormat(VkFormat requestedViewFormat, VkFormat imageFormat) {
    if (!dxvmIsBcnFormat(requestedViewFormat) || requestedViewFormat == imageFormat)
      return requestedViewFormat;

    const bool srgb = dxvmIsSrgbBcnFormat(requestedViewFormat);

    if (srgb) {
      switch (imageFormat) {
        case VK_FORMAT_R8G8B8A8_UNORM:        return VK_FORMAT_R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM:        return VK_FORMAT_B8G8R8A8_SRGB;
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:  return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:  return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
        default:                              return imageFormat;
      }
    } else {
      switch (imageFormat) {
        case VK_FORMAT_R8G8B8A8_SRGB:         return VK_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB:         return VK_FORMAT_B8G8R8A8_UNORM;
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:   return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:   return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
        default:                              return imageFormat;
      }
    }
  }

}
