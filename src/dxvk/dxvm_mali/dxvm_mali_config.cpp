#include "dxvm_mali_config.h"

namespace dxvk {

  DxvmMaliConfig::DxvmMaliConfig(const Config& config) {
    enableDynamicRendering     = config.getOption<bool>    ("dxvk.enableDynamicRendering",     true);
    enableFP16Shaders          = config.getOption<bool>    ("dxvk.enableFP16Shaders",          false);
    enableINT8Shaders          = config.getOption<bool>    ("dxvk.enableINT8Shaders",          false);
    enableDescriptorIndexing   = config.getOption<bool>    ("dxvk.enableDescriptorIndexing",   false);
    enableBufferDeviceAddress  = config.getOption<bool>    ("dxvk.enableBufferDeviceAddress",  false);
    enableShaderTileImage      = config.getOption<bool>    ("dxvk.enableShaderTileImage",      false);
    enableARMBuiltins          = config.getOption<bool>    ("dxvk.enableARMBuiltins",          false);
    enableSynchronization2     = config.getOption<bool>    ("dxvk.enableSynchronization2",     false);
    enableTimelineSemaphore    = config.getOption<bool>    ("dxvk.enableTimelineSemaphore",    false);
    preferCachedMemory         = config.getOption<Tristate>("dxvk.preferCachedMemory",         Tristate::Auto);
    maxMemoryBudgetMB          = config.getOption<int32_t> ("dxvk.maxMemoryBudget",            0);
    enableBcnTranscoding       = config.getOption<bool>    ("dxvk.enableBcnTranscoding",       true);
  }

}
