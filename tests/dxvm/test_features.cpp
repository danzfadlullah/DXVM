#include "dxvk/dxvm_mali/dxvm_mali_features.h"

#include <cstdio>

#define CHECK(cond) do {                                                  \
  if (!(cond)) {                                                          \
    std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
    ++failures;                                                           \
  }                                                                       \
} while (false)

static int failures = 0;

int main() {
  using dxvk::DxvkDeviceExtensionInfo;
  using dxvk::DxvkDeviceFeatures;
  using dxvk::dxvmMaliChainFeatures;

  // --- 1. Non-Mali: strict no-op. ---
  DxvkDeviceExtensionInfo plainExtensions{};
  DxvkDeviceFeatures      plainFeatures{};

  // The upstream feature table marks these four core features as required,
  // so seed their upstream (enabled) values. The override must not touch any
  // of them on a non-Mali device.
  plainFeatures.core.features.dualSrcBlend         = VK_TRUE;
  plainFeatures.core.features.fillModeNonSolid     = VK_TRUE;
  plainFeatures.core.features.multiViewport        = VK_TRUE;
  plainFeatures.core.features.textureCompressionBC = VK_TRUE;

  dxvmMaliChainFeatures(false, plainExtensions, plainFeatures);

  CHECK(plainFeatures.core.features.dualSrcBlend         == VK_TRUE);
  CHECK(plainFeatures.core.features.fillModeNonSolid     == VK_TRUE);
  CHECK(plainFeatures.core.features.multiViewport        == VK_TRUE);
  CHECK(plainFeatures.core.features.textureCompressionBC == VK_TRUE);

  CHECK(plainFeatures.armShaderCoreBuiltins.shaderCoreBuiltins == VK_FALSE);
  CHECK(plainFeatures.extShaderTileImage.shaderTileImageColorReadAccess == VK_FALSE);
  CHECK(plainFeatures.armRasterizationOrderAttachmentAccess.rasterizationOrderColorAttachmentAccess == VK_FALSE);
  CHECK(plainFeatures.khrFragmentShadingRate.pipelineFragmentShadingRate == VK_FALSE);
  CHECK(plainFeatures.khrSynchronization2.synchronization2 == VK_FALSE);
  CHECK(plainFeatures.khrTimelineSemaphore.timelineSemaphore == VK_FALSE);

  // --- 2. Mali: core overrides + feature enablement when extensions exist. ---
  DxvkDeviceExtensionInfo maliExtensions{};
  DxvkDeviceFeatures      maliFeatures{};

  maliFeatures.core.features.dualSrcBlend         = VK_TRUE;
  maliFeatures.core.features.fillModeNonSolid     = VK_TRUE;
  maliFeatures.core.features.multiViewport        = VK_TRUE;
  maliFeatures.core.features.textureCompressionBC = VK_TRUE;

  maliExtensions.armShaderCoreBuiltins.specVersion = 1;
  maliExtensions.khrSynchronization2.specVersion    = 1;
  maliExtensions.khrTimelineSemaphore.specVersion   = 1;

  dxvmMaliChainFeatures(true, maliExtensions, maliFeatures);

  CHECK(maliFeatures.core.features.dualSrcBlend         == VK_FALSE);
  CHECK(maliFeatures.core.features.fillModeNonSolid     == VK_FALSE);
  CHECK(maliFeatures.core.features.multiViewport        == VK_FALSE);
  CHECK(maliFeatures.core.features.textureCompressionBC == VK_FALSE);

  CHECK(maliFeatures.armShaderCoreBuiltins.shaderCoreBuiltins == VK_TRUE);
  CHECK(maliFeatures.khrSynchronization2.synchronization2 == VK_TRUE);
  CHECK(maliFeatures.khrTimelineSemaphore.timelineSemaphore == VK_TRUE);

  // --- 3. Mali: a missing extension must not enable its feature bool. ---
  DxvkDeviceExtensionInfo noArmExtensions{};
  DxvkDeviceFeatures      noArmFeatures{};

  CHECK(noArmExtensions.armShaderCoreBuiltins.specVersion == 0);

  dxvmMaliChainFeatures(true, noArmExtensions, noArmFeatures);

  CHECK(noArmFeatures.armShaderCoreBuiltins.shaderCoreBuiltins == VK_FALSE);
  CHECK(noArmFeatures.extShaderTileImage.shaderTileImageColorReadAccess == VK_FALSE);
  CHECK(noArmFeatures.armRasterizationOrderAttachmentAccess.rasterizationOrderColorAttachmentAccess == VK_FALSE);
  CHECK(noArmFeatures.khrFragmentShadingRate.pipelineFragmentShadingRate == VK_FALSE);
  CHECK(noArmFeatures.khrSynchronization2.synchronization2 == VK_FALSE);
  CHECK(noArmFeatures.khrTimelineSemaphore.timelineSemaphore == VK_FALSE);

  if (failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }

  std::printf("test_features: OK\n");
  return 0;
}
