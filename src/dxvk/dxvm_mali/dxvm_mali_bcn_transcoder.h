#pragma once

#include "../dxvk_include.h"
#include "../dxvk_buffer.h"
#include "bcndecode/bcn_decode.h"
#include "dxvm_mali_bcn_layout.h"

namespace dxvk {

  /**
   * \brief BCn transcoder for Mali
   *
   * Decodes BCn source blocks into RGBA8. The CPU path is always available
   * and is used when no GPU dispatch pipeline is active; this guarantees
   * the output format matches the (remapped) image format exactly.
   */
  class DxvmBcnTranscoder {
  public:
    DxvmBcnTranscoder();

    /** True when GPU compute dispatch is unavailable and CPU decode is used. */
    bool useCpuFallback() const { return m_useCpuFallback; }

    /**
     * Decode a BCn source region into an RGBA8 destination buffer.
     * Returns VK_SUCCESS on success.
     */
    VkResult transcode(
            const Rc<DxvkBuffer>& src,
            VkDeviceSize          srcOffset,
            VkFormat              bcnFormat,
            const Rc<DxvkBuffer>& dst,
            VkDeviceSize          dstOffset,
            uint32_t              imageWidth,
            uint32_t              imageHeight);

  private:
    bool m_useCpuFallback = true;
  };

}
