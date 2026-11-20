#include "dxvk/dxvm_mali/dxvm_mali_context.h"

#include <iostream>

#define CHECK(cond) do {                                                  \
  if (!(cond)) {                                                          \
    std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
    ++failures;                                                           \
  }                                                                       \
} while (false)

static int failures = 0;

int main() {
  dxvk::Config opts;
  opts.setOption("dxvk.enableARMBuiltins", "false");

  dxvk::DxvmMaliContext ctx(true, opts);
  CHECK(ctx.isMali() == true);
  CHECK(ctx.config().enableARMBuiltins == false);

  dxvk::DxvmMaliContext other(false, dxvk::Config());
  CHECK(other.isMali() == false);
  CHECK(other.config().enableBcnTranscoding == true);

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }

  std::cout << "test_context: OK\n";
  return 0;
}
