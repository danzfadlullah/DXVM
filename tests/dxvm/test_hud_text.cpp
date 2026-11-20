#include "dxvk/hud/dxvk_hud_dxvm_status.h"
#include <cstdio>
#include <string>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

int main() {
  using dxvk::hud::dxvmHudStatusLine;
  CHECK(dxvmHudStatusLine(false, true, true) == "BCn→ASTC:GPU · DynR:ON · Sync2:ON");
  CHECK(dxvmHudStatusLine(true, true, true) == "BCn→ASTC:CPU · DynR:ON · Sync2:ON");
  CHECK(dxvmHudStatusLine(false, false, false) == "BCn→ASTC:GPU · DynR:OFF · Sync2:OFF");
  if (failures) return 1;
  std::printf("test_hud_text: OK\n");
  return 0;
}
