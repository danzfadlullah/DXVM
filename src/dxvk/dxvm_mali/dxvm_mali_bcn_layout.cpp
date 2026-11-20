#include "dxvm_mali_bcn_layout.h"

#include <algorithm>

namespace dxvk {

  namespace {

    VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) {
      return alignment ? (value + alignment - 1u) / alignment * alignment : value;
    }

    VkExtent3D mipExtent(VkExtent3D base, uint32_t level) {
      base.width  = std::max(1u, base.width  >> level);
      base.height = std::max(1u, base.height >> level);
      base.depth  = std::max(1u, base.depth  >> level);
      return base;
    }

    uint32_t blockCount(uint32_t extent, uint32_t blockSize) {
      return (extent + blockSize - 1u) / blockSize;
    }

  }

  VkDeviceSize dxvmBcnBlockBytes(VkFormat bcnFormat) {
    switch (bcnFormat) {
      case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
      case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
      case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
      case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
      case VK_FORMAT_BC4_UNORM_BLOCK:
      case VK_FORMAT_BC4_SNORM_BLOCK:
        return 8u;
      case VK_FORMAT_BC2_UNORM_BLOCK:
      case VK_FORMAT_BC2_SRGB_BLOCK:
      case VK_FORMAT_BC3_UNORM_BLOCK:
      case VK_FORMAT_BC3_SRGB_BLOCK:
      case VK_FORMAT_BC5_UNORM_BLOCK:
      case VK_FORMAT_BC5_SNORM_BLOCK:
      case VK_FORMAT_BC6H_UFLOAT_BLOCK:
      case VK_FORMAT_BC6H_SFLOAT_BLOCK:
      case VK_FORMAT_BC7_UNORM_BLOCK:
      case VK_FORMAT_BC7_SRGB_BLOCK:
        return 16u;
      default:
        return 0u;
    }
  }

  VkDeviceSize dxvmBcnSubresourceSize(VkFormat bcnFormat, VkExtent3D extent) {
    VkDeviceSize blockBytes = dxvmBcnBlockBytes(bcnFormat);
    if (!blockBytes)
      return 0u;

    uint32_t blocks = blockCount(extent.width, 4u)
                    * blockCount(extent.height, 4u)
                    * std::max(1u, extent.depth);
    return VkDeviceSize(blocks) * blockBytes;
  }

  DxvmBcnSubresourceLayout dxvmBcnSubresourceAt(
          VkFormat      bcnFormat,
          VkExtent3D    baseExtent,
          uint32_t      mipLevels,
          uint32_t      numLayers,
          VkDeviceSize  baseSrcOffset,
          VkDeviceSize  srcAlignment,
          VkDeviceSize  dstAlignment,
          uint32_t      mip,
          uint32_t      layer) {
    DxvmBcnSubresourceLayout result;
    if (mip >= mipLevels || layer >= numLayers || !dxvmBcnBlockBytes(bcnFormat))
      return result;

    VkDeviceSize srcOffset = baseSrcOffset;
    VkDeviceSize dstOffset = 0u;

    for (uint32_t m = 0u; m < mip; m++) {
      VkExtent3D extent = mipExtent(baseExtent, m);
      VkDeviceSize srcSize = dxvmBcnSubresourceSize(bcnFormat, extent);
      VkDeviceSize dstSize = VkDeviceSize(extent.width) * extent.height * extent.depth * 4u;
      srcOffset += VkDeviceSize(numLayers) * alignUp(srcSize, srcAlignment);
      dstOffset += VkDeviceSize(numLayers) * alignUp(dstSize, dstAlignment);
    }

    result.extent = mipExtent(baseExtent, mip);

    result.srcSize = dxvmBcnSubresourceSize(bcnFormat, result.extent);
    result.dstSize = VkDeviceSize(result.extent.width) * result.extent.height
                   * result.extent.depth * 4u;

    srcOffset += VkDeviceSize(layer) * alignUp(result.srcSize, srcAlignment);
    dstOffset += VkDeviceSize(layer) * alignUp(result.dstSize, dstAlignment);

    result.srcOffset = srcOffset;
    result.dstOffset = dstOffset;
    result.valid = true;
    return result;
  }

  VkDeviceSize dxvmBcnStagingSize(
          VkExtent3D    baseExtent,
          uint32_t      mipLevels,
          uint32_t      numLayers,
          VkDeviceSize  dstAlignment) {
    VkDeviceSize size = 0u;

    for (uint32_t m = 0u; m < mipLevels; m++) {
      VkExtent3D extent = mipExtent(baseExtent, m);
      VkDeviceSize dstSize = VkDeviceSize(extent.width) * extent.height * extent.depth * 4u;
      size += VkDeviceSize(numLayers) * alignUp(dstSize, dstAlignment);
    }

    return size;
  }

}
