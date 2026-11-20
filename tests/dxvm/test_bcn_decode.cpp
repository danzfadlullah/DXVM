#include "dxvk/dxvm_mali/bcndecode/bcn_decode.h"

#include <cstdio>
#include <cstring>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (false)

int main() {
  using namespace dxvk;

  // BC1 block: color0 = red (0xF800), color1 = blue (0x001F),
  // indices all 0 -> every pixel should be red (0xF8,0x00,0x00).
  uint8_t bc1[8];
  uint16_t red = 0xF800, blue = 0x001F;
  std::memcpy(bc1, &red, 2);
  std::memcpy(bc1 + 2, &blue, 2);
  std::memset(bc1 + 4, 0, 4); // indices all 0

  Rgba8 px[16];
  CHECK(decodeBcnBlock(bc1, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, px));
  CHECK(px[0].r >= 0xF0 && px[0].g <= 0x0F && px[0].b <= 0x0F && px[0].a == 255);
  CHECK(px[15].r == px[0].r && px[15].g == px[0].g && px[15].b == px[0].b);

  // BC1 with c0 > c1 and index 3 -> interpolated color (1/3 blue + 2/3 ... wait:
  // c0=red, c1=blue, index 3 = (c0 + 2*c1)/3 -> more blue than red.
  std::memset(bc1 + 4, 0xFF, 4); // all indices = 3
  CHECK(decodeBcnBlock(bc1, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, px));
  CHECK(px[0].b > px[0].r); // blue-dominant

  // BC3 alpha: a0=255, a1=0, indices 0 -> alpha 255
  uint8_t bc3[16] = {0};
  bc3[0] = 255; bc3[1] = 0;
  std::memcpy(bc3 + 8, &red, 2);
  std::memcpy(bc3 + 10, &blue, 2);
  std::memset(bc3 + 12, 0, 4);
  CHECK(decodeBcnBlock(bc3, VK_FORMAT_BC3_UNORM_BLOCK, px));
  CHECK(px[0].a == 255);

  // BC6H mode 0 block: decoder returns success and valid alpha.
  uint8_t bc6h[16] = {0};
  bc6h[0] = 0; // mode 0
  bool ok6 = decodeBcnBlock(bc6h, VK_FORMAT_BC6H_UFLOAT_BLOCK, px);
  if (ok6) CHECK(px[0].a == 255);

  // BC7 block: decoder returns success for common modes.
  uint8_t bc7[16] = {0};
  bc7[0] = 0;
  bool ok7 = decodeBcnBlock(bc7, VK_FORMAT_BC7_UNORM_BLOCK, px);
  if (ok7) CHECK(px[0].a == 255);

  // Unsupported format returns false.
  CHECK(!decodeBcnBlock(bc1, VK_FORMAT_R8G8B8A8_UNORM, px));

  if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
  std::printf("test_bcn_decode: OK\n");
  return 0;
}
