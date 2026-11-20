#include "dxvk/dxvm_mali/dxvm_mali_hints.h"

#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (false)

int main() {
  using namespace dxvk;

  DxvmMaliConfig cfg(Config{});

  // Non-Mali: no hints.
  CHECK(dxvmMaliHintFlags(cfg, VK_DRIVER_ID_NVIDIA_PROPRIETARY, false) == 0);
  CHECK(dxvmMaliHintFlags(cfg, VK_DRIVER_ID_AMD_PROPRIETARY_KHR, true) == 0);

  // Mali + Valhall: dynamic rendering, compute mipgen, descriptor offsets.
  uint32_t f = dxvmMaliHintFlags(cfg, VK_DRIVER_ID_ARM_PROPRIETARY, true);
  CHECK((f & DXVM_HINT_DYNAMIC_RENDERING) != 0);
  CHECK((f & DXVM_HINT_COMPUTE_MIPGEN) != 0);
  CHECK((f & DXVM_HINT_DESCRIPTOR_OFFSETS) != 0);

  // Mali + non-Valhall: no compute mipgen.
  f = dxvmMaliHintFlags(cfg, VK_DRIVER_ID_MESA_PANVK, false);
  CHECK((f & DXVM_HINT_COMPUTE_MIPGEN) == 0);

  // 16-bit shader arithmetic on Mali.
  // Non-Mali with explicit FP16 option still gets 16-bit (upstream behavior).
  DxvmMaliConfig nonMaliCfg(Config{});
  nonMaliCfg.enableFP16Shaders = true;
  CHECK(dxvmMaliUse16Bit(nonMaliCfg, false, true, false) == true);
  CHECK(dxvmMaliUse16Bit(cfg, false, true, true) == false);
  CHECK(dxvmMaliUse16Bit(cfg, true, true, true) == true);
  CHECK(dxvmMaliUse16Bit(cfg, true, false, false) == false);

  if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
  std::printf("test_hints: OK\n");
  return 0;
}
