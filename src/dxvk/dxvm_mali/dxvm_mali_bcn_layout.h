#pragma once

#include <vulkan/vulkan.h>

namespace dxvk {

  /**
   * \brief Layout of one subresource in a BCn upload chain
   *
   * Source subresources are BCn blocks; the transcoded destination holds
   * linear RGBA8 pixels. Both are padded to their respective alignment so
   * that offsets for arbitrary mip/array-layer pairs can be computed without
   * walking the whole chain at upload time.
   */
  struct DxvmBcnSubresourceLayout {
    VkExtent3D   extent    = { 1u, 1u, 1u };
    VkDeviceSize srcSize   = 0u;
    VkDeviceSize dstSize   = 0u;
    VkDeviceSize srcOffset = 0u;
    VkDeviceSize dstOffset = 0u;
    bool         valid     = false;
  };

  /** Bytes per 4x4 block for a BCn format; zero for non-BCn formats. */
  VkDeviceSize dxvmBcnBlockBytes(VkFormat bcnFormat);

  /** Size of one BCn subresource in bytes (block-padded). */
  VkDeviceSize dxvmBcnSubresourceSize(VkFormat bcnFormat, VkExtent3D extent);

  /**
   * Compute the source/destination offsets of a single mip/array-layer pair.
   * Subresources are laid out mip-major, layer-minor, matching the staging
   * layout produced by the D3D9/D3D11 initializers.
   */
  DxvmBcnSubresourceLayout dxvmBcnSubresourceAt(
          VkFormat      bcnFormat,
          VkExtent3D    baseExtent,
          uint32_t      mipLevels,
          uint32_t      numLayers,
          VkDeviceSize  baseSrcOffset,
          VkDeviceSize  srcAlignment,
          VkDeviceSize  dstAlignment,
          uint32_t      mip,
          uint32_t      layer);

  /** Total size of the RGBA8 staging buffer for all subresources. */
  VkDeviceSize dxvmBcnStagingSize(
          VkExtent3D    baseExtent,
          uint32_t      mipLevels,
          uint32_t      numLayers,
          VkDeviceSize  dstAlignment);

}
