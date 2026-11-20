#include "dxvk/dxvm_mali/dxvm_mali_bcn_layout.h"

#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (false)

int main() {
  using namespace dxvk;

  // BC3: 16 bytes per 4x4 block. BC1/BC4: 8 bytes per block.
  CHECK(dxvmBcnBlockBytes(VK_FORMAT_BC3_UNORM_BLOCK) == 16u);
  CHECK(dxvmBcnBlockBytes(VK_FORMAT_BC1_RGBA_SRGB_BLOCK) == 8u);
  CHECK(dxvmBcnBlockBytes(VK_FORMAT_R8G8B8A8_UNORM) == 0u);

  // Block-padded source sizes, including a non-multiple-of-4 extent.
  CHECK(dxvmBcnSubresourceSize(VK_FORMAT_BC3_UNORM_BLOCK, { 64u, 64u, 1u }) == 4096u);
  CHECK(dxvmBcnSubresourceSize(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, { 5u, 5u, 1u }) == 32u);

  // 64x64 BC3 texture, 4 mips, 2 array layers, 256-byte subresource alignment.
  // The source chain matches what the D3D11 initializer writes: for each mip,
  // layer 0 then layer 1, each padded to the alignment. RGBA8 destination
  // subresources are laid out in the same order in the staging buffer.
  const VkFormat fmt = VK_FORMAT_BC3_UNORM_BLOCK;
  const VkExtent3D base = { 64u, 64u, 1u };
  const uint32_t mipLevels = 4u;
  const uint32_t numLayers = 2u;
  const VkDeviceSize srcAlign = 256u;
  const VkDeviceSize dstAlign = 256u;
  const VkDeviceSize baseOffset = 1000u;

  const VkDeviceSize expectedSrcSize[4] = { 4096u, 1024u, 256u, 64u };
  const VkDeviceSize expectedDstSize[4] = { 16384u, 4096u, 1024u, 256u };
  const VkDeviceSize expectedSrcOffset[4][2] = {
    { 1000u,  5096u },
    { 9192u, 10216u },
    {11240u, 11496u },
    {11752u, 12008u },
  };
  const VkDeviceSize expectedDstOffset[4][2] = {
    {     0u,   16384u },
    { 32768u,   36864u },
    { 40960u,   41984u },
    { 43008u,   43264u },
  };

  CHECK(dxvmBcnStagingSize(base, mipLevels, numLayers, dstAlign) == 43520u);

  for (uint32_t m = 0u; m < mipLevels; m++) {
    for (uint32_t l = 0u; l < numLayers; l++) {
      DxvmBcnSubresourceLayout sub = dxvmBcnSubresourceAt(
        fmt, base, mipLevels, numLayers, baseOffset,
        srcAlign, dstAlign, m, l);

      CHECK(sub.valid);
      CHECK(sub.extent.width == (64u >> m));
      CHECK(sub.extent.height == (64u >> m));
      CHECK(sub.srcSize == expectedSrcSize[m]);
      CHECK(sub.dstSize == expectedDstSize[m]);
      CHECK(sub.srcOffset == expectedSrcOffset[m][l]);
      CHECK(sub.dstOffset == expectedDstOffset[m][l]);
      CHECK(sub.dstOffset + sub.dstSize <= 43520u);

      // Source subresources must never overlap each other.
      for (uint32_t mm = 0u; mm < mipLevels; mm++) {
        for (uint32_t ll = 0u; ll < numLayers; ll++) {
          if (mm == m && ll == l) continue;
          DxvmBcnSubresourceLayout other = dxvmBcnSubresourceAt(
            fmt, base, mipLevels, numLayers, baseOffset,
            srcAlign, dstAlign, mm, ll);
          CHECK(other.valid);
          CHECK(sub.srcOffset >= other.srcOffset + other.srcSize
             || other.srcOffset >= sub.srcOffset + sub.srcSize);
          CHECK(sub.dstOffset >= other.dstOffset + other.dstSize
             || other.dstOffset >= sub.dstOffset + sub.dstSize);
        }
      }
    }
  }

  // Out-of-range subresources are rejected instead of reading stray memory.
  CHECK(!dxvmBcnSubresourceAt(fmt, base, mipLevels, numLayers,
    baseOffset, srcAlign, dstAlign, 4u, 0u).valid);
  CHECK(!dxvmBcnSubresourceAt(fmt, base, mipLevels, numLayers,
    baseOffset, srcAlign, dstAlign, 0u, 2u).valid);

  if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
  std::printf("test_bcn_subresources: OK\n");
  return 0;
}
