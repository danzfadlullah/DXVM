#include "dxvk/dxvm_mali/dxvm_mali_format_policy.h"

#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (false)

int main() {
  using namespace dxvk;

  // Non-Mali: identity.
  CHECK(dxvmMaliRemapFormat(false, false, true, VK_FORMAT_BC3_UNORM_BLOCK) == VK_FORMAT_BC3_UNORM_BLOCK);
  CHECK(dxvmMaliRemapFormat(false, true, true, VK_FORMAT_BC1_RGBA_SRGB_BLOCK) == VK_FORMAT_BC1_RGBA_SRGB_BLOCK);

  // Mali + GPU dispatch: ASTC, sRGB preserved.
  CHECK(dxvmMaliRemapFormat(true, false, true, VK_FORMAT_BC1_RGBA_UNORM_BLOCK) == VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
  CHECK(dxvmMaliRemapFormat(true, false, true, VK_FORMAT_BC1_RGBA_SRGB_BLOCK) == VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
  CHECK(dxvmMaliRemapFormat(true, false, true, VK_FORMAT_BC3_SRGB_BLOCK) == VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
  CHECK(dxvmMaliRemapFormat(true, false, true, VK_FORMAT_BC6H_SFLOAT_BLOCK) == VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK);
  CHECK(dxvmMaliRemapFormat(true, false, true, VK_FORMAT_BC7_UNORM_BLOCK) == VK_FORMAT_ASTC_6x6_UNORM_BLOCK);
  CHECK(dxvmMaliRemapFormat(true, false, true, VK_FORMAT_BC7_SRGB_BLOCK) == VK_FORMAT_ASTC_6x6_SRGB_BLOCK);

  // Mali + CPU fallback: RGBA8, sRGB preserved.
  CHECK(dxvmMaliRemapFormat(true, true, true, VK_FORMAT_BC3_UNORM_BLOCK) == VK_FORMAT_R8G8B8A8_UNORM);
  CHECK(dxvmMaliRemapFormat(true, true, true, VK_FORMAT_BC3_SRGB_BLOCK) == VK_FORMAT_R8G8B8A8_SRGB);

  // dxvk.enableBcnTranscoding=false keeps the original BCn format (identity).
  CHECK(dxvmMaliRemapFormat(true, true, false, VK_FORMAT_BC3_SRGB_BLOCK) == VK_FORMAT_BC3_SRGB_BLOCK);
  CHECK(dxvmMaliRemapFormat(true, false, false, VK_FORMAT_BC1_RGBA_UNORM_BLOCK) == VK_FORMAT_BC1_RGBA_UNORM_BLOCK);

  // View formats keep the requested color space (never collapse sRGB -> linear).
  CHECK(dxvmMaliRemapViewFormat(VK_FORMAT_BC3_SRGB_BLOCK, VK_FORMAT_R8G8B8A8_UNORM) == VK_FORMAT_R8G8B8A8_SRGB);
  CHECK(dxvmMaliRemapViewFormat(VK_FORMAT_BC3_SRGB_BLOCK, VK_FORMAT_ASTC_4x4_UNORM_BLOCK) == VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
  CHECK(dxvmMaliRemapViewFormat(VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_R8G8B8A8_SRGB) == VK_FORMAT_R8G8B8A8_UNORM);
  CHECK(dxvmMaliRemapViewFormat(VK_FORMAT_BC7_SRGB_BLOCK, VK_FORMAT_ASTC_6x6_UNORM_BLOCK) == VK_FORMAT_ASTC_6x6_SRGB_BLOCK);
  CHECK(dxvmMaliRemapViewFormat(VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM) == VK_FORMAT_R8G8B8A8_UNORM);
  CHECK(dxvmIsSrgbBcnFormat(VK_FORMAT_BC1_RGBA_SRGB_BLOCK));
  CHECK(!dxvmIsSrgbBcnFormat(VK_FORMAT_BC1_RGBA_UNORM_BLOCK));

  // Non-BCN: untouched.
  CHECK(dxvmMaliRemapFormat(true, false, true, VK_FORMAT_R8G8B8A8_UNORM) == VK_FORMAT_R8G8B8A8_UNORM);
  CHECK(dxvmIsBcnFormat(VK_FORMAT_BC7_SRGB_BLOCK));
  CHECK(!dxvmIsBcnFormat(VK_FORMAT_R8G8B8A8_UNORM));

  if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
  std::printf("test_format: OK\n");
  return 0;
}
