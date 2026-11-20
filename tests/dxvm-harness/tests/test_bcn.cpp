#include "../src/scenes/Bcn.h"

#include <cstdio>
#include <string>
#include <set>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

int main() {
  using namespace dxvmt;

  const auto names = bcnSceneNames();

  // Seven base BCn formats plus their sRGB variants.
  CHECK(names.size() == 12);

  std::set<std::string> s(names.begin(), names.end());
  CHECK(s.count("bcn.bc1")  == 1);
  CHECK(s.count("bcn.bc2")  == 1);
  CHECK(s.count("bcn.bc3")  == 1);
  CHECK(s.count("bcn.bc4")  == 1);
  CHECK(s.count("bcn.bc5")  == 1);
  CHECK(s.count("bcn.bc6h") == 1);
  CHECK(s.count("bcn.bc7")  == 1);
  CHECK(s.count("bcn.bc1_srgb") == 1);
  CHECK(s.count("bcn.bc7_srgb") == 1);

  // Every BCn scene must be flagged as needing the remap path.
  for (const auto& n : names)
    CHECK(bcnNeedsRemap(n));

  CHECK(!bcnNeedsRemap("texture"));
  CHECK(!bcnNeedsRemap("mipmaps"));

  if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
  std::printf("test_bcn: OK\n");
  return 0;
}
