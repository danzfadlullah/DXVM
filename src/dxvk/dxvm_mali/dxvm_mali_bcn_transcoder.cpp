#include "dxvm_mali_bcn_transcoder.h"
#include "dxvm_mali_format_policy.h"

#include <cstring>

namespace dxvk {

  DxvmBcnTranscoder::DxvmBcnTranscoder() {
    // GPU compute dispatch requires descriptor-set wiring into the context's
    // transient allocator. Until that lands, the CPU decoder is the reliable
    // path: it always produces RGBA8 matching the remapped image format.
    m_useCpuFallback = true;
  }

  VkResult DxvmBcnTranscoder::transcode(
          const Rc<DxvkBuffer>& src,
          VkDeviceSize          srcOffset,
          VkFormat              bcnFormat,
          const Rc<DxvkBuffer>& dst,
          VkDeviceSize          dstOffset,
          uint32_t              imageWidth,
          uint32_t              imageHeight) {
    if (!src || !dst)
      return VK_ERROR_INVALID_EXTERNAL_HANDLE;

    const DxvkFormatInfo* info = lookupFormatInfo(bcnFormat);
    if (!info || !dxvmIsBcnFormat(bcnFormat))
      return VK_ERROR_FORMAT_NOT_SUPPORTED;

    void* srcPtr = src->mapPtr(srcOffset);
    void* dstPtr = dst->mapPtr(dstOffset);
    VkDeviceSize available = src->info().size - srcOffset;
    if (!srcPtr || !dstPtr)
      return VK_ERROR_MEMORY_MAP_FAILED;

    VkDeviceSize blockSize = info->elementSize;
    VkDeviceSize blocks = available / blockSize;
    Rgba8 pixels[16];
    uint8_t* out = static_cast<uint8_t*>(dstPtr);
    uint32_t blocksX = (imageWidth + 3u) / 4u;

    for (VkDeviceSize b = 0; b < blocks; b++) {
      const uint8_t* block = static_cast<const uint8_t*>(srcPtr) + b * blockSize;
      if (!decodeBcnBlock(block, bcnFormat, pixels))
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
      uint32_t bx = uint32_t(b % blocksX), by = uint32_t(b / blocksX);
      // Scatter 4x4 block into linear RGBA8 rows.
      for (uint32_t y = 0; y < 4u; y++) {
        uint32_t py = by * 4u + y;
        if (py >= imageHeight) continue;
        for (uint32_t x = 0; x < 4u; x++) {
          uint32_t px = bx * 4u + x;
          if (px >= imageWidth) continue;
          std::memcpy(out + (uint64_t(py) * imageWidth + px) * 4u,
                      &pixels[y * 4u + x], 4u);
        }
      }
    }

    return VK_SUCCESS;
  }

}
