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
          VkDeviceSize          dstOffset) {
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

    for (VkDeviceSize b = 0; b < blocks; b++) {
      const uint8_t* block = static_cast<const uint8_t*>(srcPtr) + b * blockSize;
      if (!decodeBcnBlock(block, bcnFormat, pixels))
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
      std::memcpy(out + b * 64, pixels, 64); // 16 RGBA8 = 64 bytes
    }

    return VK_SUCCESS;
  }

}
