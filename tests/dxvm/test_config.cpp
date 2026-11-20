#include "dxvk/dxvm_mali/dxvm_mali_config.h"

#include <iostream>

#define CHECK(cond) do {                                                  \
  if (!(cond)) {                                                          \
    std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
    ++failures;                                                           \
  }                                                                       \
} while (false)

static int failures = 0;

int main() {
  using dxvk::Config;
  using dxvk::DxvmMaliConfig;

  // Defaults for a fresh, empty Config, per task-1 brief.
  DxvmMaliConfig cfg(Config{});
  CHECK(cfg.enableBcnTranscoding   == true);
  CHECK(cfg.enableDynamicRendering == true);
  CHECK(cfg.maxMemoryBudgetMB      == 0);
  CHECK(cfg.enableSynchronization2 == false);

  // Remaining fallbacks copied verbatim from dxvk_options.cpp.
  CHECK(cfg.enableFP16Shaders         == false);
  CHECK(cfg.enableINT8Shaders         == false);
  CHECK(cfg.enableDescriptorIndexing   == false);
  CHECK(cfg.enableBufferDeviceAddress  == false);
  CHECK(cfg.enableShaderTileImage      == false);
  CHECK(cfg.enableTimelineSemaphore    == false);
  CHECK(cfg.preferCachedMemory         == dxvk::Tristate::Auto);

  // An explicitly set option overrides its fallback.
  Config opts;
  opts.setOption("dxvk.enableARMBuiltins", "false");
  DxvmMaliConfig overridden(opts);
  CHECK(overridden.enableARMBuiltins == false);

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }

  std::cout << "test_config: OK\n";
  return 0;
}
