#include "../src/scenes/Msaa.h"

#include <cstdio>
#include <string>

static int failures = 0;

#define CHECK(cond) do {                                                  \
  if (!(cond)) {                                                          \
    std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
    ++failures;                                                           \
  }                                                                       \
} while (false)

int main() {
  using namespace dxvmt;

  // Pinned from the owner's real DXML run on Mali-G615 MC2 driver r44p1.
  // DXML's own engine reports these red values and passes.
  CHECK(expectedSampleIndexRamp(2)  == 9);
  CHECK(expectedSampleIndexRamp(4)  == 26);
  CHECK(expectedSampleIndexRamp(8)  == 60);
  CHECK(expectedSampleIndexRamp(16) == 128);

  // Unsupported counts have no expectation and must not be guessed.
  CHECK(expectedSampleIndexRamp(1)  == -1);
  CHECK(expectedSampleIndexRamp(3)  == -1);
  CHECK(expectedSampleIndexRamp(32) == -1);

  // The three formats the reference run covers per sample count.
  const auto formats = msaaFormats();
  CHECK(formats.size() == 3);
  CHECK(std::string(formats[0].suffix) == "rgba8");
  CHECK(std::string(formats[1].suffix) == "rgba16f");
  CHECK(std::string(formats[2].suffix) == "bgra8");

  // Coverage expectation: 64 partial pixels along the diagonal for every count.
  CHECK(msaaPartialPixelExpectation() == 64);

  // Reference pixel values from the same run.
  CHECK(msaaPositionExpectation().first  == 42);
  CHECK(msaaPositionExpectation().second == 82);
  CHECK(msaaTexture2dmsExpectation() == std::string("128,64,255"));

  if (failures) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }

  std::printf("test_msaa_ramp: OK\n");
  return 0;
}