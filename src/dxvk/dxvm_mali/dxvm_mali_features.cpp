#include "dxvm_mali_features.h"

namespace dxvk {

  void dxvmMaliChainFeatures(
          bool                    isMali,
          DxvkDeviceExtensionInfo&  extensions,
          DxvkDeviceFeatures&       features) {
    // Non-Mali: strict no-op — upstream feature table stays authoritative.
    if (!isMali)
      return;

    // Core features Mali-G615 does not support are already marked optional
    // (require=false) in the baseline dxvk_device_info.cpp feature table.
    // We only ensure they stay disabled on Mali; the state-emulation layer
    // provides fallbacks. We do NOT change their `required` status here —
    // that lives in the feature table and must remain device-enumerating.
    features.core.features.dualSrcBlend         = VK_FALSE;
    features.core.features.fillModeNonSolid     = VK_FALSE;
    features.core.features.multiViewport        = VK_FALSE;
    features.core.features.textureCompressionBC = VK_FALSE;

    // Optional ARM/KHR features: the baseline feature table already chains
    // their structs into pNext and populates specVersion from the device.
    // Here we only gate the feature bits: enable when the extension is
    // actually reported as supported (specVersion != 0).
    if (extensions.armShaderCoreBuiltins.specVersion)
      features.armShaderCoreBuiltins.shaderCoreBuiltins = VK_TRUE;

    if (extensions.extShaderTileImage.specVersion)
      features.extShaderTileImage.shaderTileImageColorReadAccess = VK_TRUE;

    if (extensions.armRasterizationOrderAttachmentAccess.specVersion)
      features.armRasterizationOrderAttachmentAccess.rasterizationOrderColorAttachmentAccess = VK_TRUE;

    if (extensions.khrFragmentShadingRate.specVersion)
      features.khrFragmentShadingRate.pipelineFragmentShadingRate = VK_TRUE;

    if (extensions.khrSynchronization2.specVersion)
      features.khrSynchronization2.synchronization2 = VK_TRUE;

    if (extensions.khrTimelineSemaphore.specVersion)
      features.khrTimelineSemaphore.timelineSemaphore = VK_TRUE;
  }

}
