#include "dxvk/dxvm_mali/dxvm_mali_vendor.h"
#include <cstdio>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

int main() {
  using namespace dxvk;
  CHECK(dxvmIsClassicVendor(0x13b5u)); // ARM Mali
  CHECK(dxvmIsClassicVendor(0x1002u)); // AMD
  CHECK(dxvmIsClassicVendor(0x10deu)); // NVIDIA
  CHECK(dxvmIsClassicVendor(0x8086u)); // Intel
  CHECK(!dxvmIsClassicVendor(0xdeadu)); // random
  if (failures) return 1;
  std::printf("test_vendor: OK\n");
  return 0;
}
