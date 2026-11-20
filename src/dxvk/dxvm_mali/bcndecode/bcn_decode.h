#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace dxvk {

  struct Rgba8 { uint8_t r, g, b, a; };

  /**
   * \brief Decode a single BCn block into 16 RGBA8 pixels (4x4).
   *
   * \param block   Pointer to the compressed block (8 or 16 bytes depending on format)
   * \param format  BCn format (BC1..BC7)
   * \param out     Output array of at least 16 RGBA8 pixels
   * \return true on success, false for unsupported format
   */
  bool decodeBcnBlock(const uint8_t* block, VkFormat format, Rgba8* out);

}
