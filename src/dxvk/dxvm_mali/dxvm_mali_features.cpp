#include "dxvm_mali_features.h"

namespace dxvk {

  void dxvmMaliChainFeatures(
          bool                    isMali,
          DxvkDeviceExtensionInfo&  extensions,
          DxvkDeviceFeatures&       features) {
    // Non-Mali: strict no-op — upstream feature table stays authoritative.
    if (!isMali)
      return;

    // Core features Mali-G615 does not support. DXVM emulates them in the
    // state-emulation layer; see the clean architecture spec §4.6.
    features.core.features.dualSrcBlend         = VK_FALSE;
    features.core.features.fillModeNonSolid     = VK_FALSE;
    features.core.features.multiViewport        = VK_FALSE;
    features.core.features.textureCompressionBC = VK_FALSE;

    // Optional ARM/KHR features: enable only when the extension is reported.
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
