#include "../src/Device.h"

#include <cstdio>

static int failures = 0;

#define CHECK(cond) do {                                                  \
  if (!(cond)) {                                                          \
    std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
    ++failures;                                                           \
  }                                                                       \
} while (false)

int main() {
  using namespace dxvmt;

  // The device line DXML prints before a suite's results. The argument is the
  // raw D3D_FEATURE_LEVEL value, so this stays testable off Windows.
  CHECK(formatDeviceLine(0xb100u) == "device created, feature level 0xb100");
  CHECK(formatDeviceLine(0xb000u) == "device created, feature level 0xb000");
  CHECK(formatDeviceLine(0xa100u) == "device created, feature level 0xa100");
  CHECK(formatDeviceLine(0xa000u) == "device created, feature level 0xa000");

  // A device that was never created must never look like a success line.
  CHECK(formatDeviceLine(0u) == "device creation failed, feature level 0x0000");

  // D3D9 and D3D10 print their feature level differently from D3D11.
  CHECK(formatDeviceLine9() == "device created");
  CHECK(formatDeviceLine9Failed() == "device creation failed");
  CHECK(formatDeviceLine10(0xa100u) == "device created, feature level 10_1");
  CHECK(formatDeviceLine10(0xa000u) == "device created, feature level 10_0");
  CHECK(formatDeviceLine10Failed() == "device creation failed");

  if (failures) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }

  std::printf("test_device: OK\n");
  return 0;
}