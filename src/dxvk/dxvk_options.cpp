#include "dxvk_options.h"
#include "dxvk_adapter.h"

namespace dxvk {

  DxvkOptions::DxvkOptions(const Config& config) {
    // Note: isMali check requires adapter, which isn't available here yet.
    // Mali-specific defaults are applied in DxvkDevice::determineDeviceHints()
    // based on adapter detection.
    
    enableDebugUtils      = config.getOption<bool>    ("dxvk.enableDebugUtils",       false);
    enableMemoryDefrag    = config.getOption<Tristate>("dxvk.enableMemoryDefrag",     Tristate::Auto);
    numCompilerThreads    = config.getOption<int32_t> ("dxvk.numCompilerThreads",     0);
    enableGraphicsPipelineLibrary = config.getOption<Tristate>("dxvk.enableGraphicsPipelineLibrary", Tristate::Auto);
    enableDescriptorHeap  = config.getOption<Tristate>("dxvk.enableDescriptorHeap",   Tristate::Auto);
    enableDescriptorBuffer = config.getOption<Tristate>("dxvk.enableDescriptorBuffer", Tristate::Auto);
    enableDescriptorUpdateTemplates = config.getOption<bool>("dxvk.enableDescriptorUpdateTemplates", false);
    enableUnifiedImageLayout = config.getOption<bool> ("dxvk.enableUnifiedImageLayouts", true);
    enableImplicitResolves = config.getOption<bool>   ("dxvk.enableImplicitResolves", true);
    enableNvRawAccessChains = config.getOption<bool>  ("dxvk.enableNvRawAccessChains", true);
    enableNvCudaInterop   = config.getOption<bool>    ("dxvk.enableNvCudaInterop",    true);
    enableNvLowLatency2   = config.getOption<Tristate>("dxvk.enableNvLowLatency2",    Tristate::Auto);
    enablePresentTiming   = config.getOption<bool>    ("dxvk.enablePresentTiming",    true);
    trackPipelineLifetime = config.getOption<Tristate>("dxvk.trackPipelineLifetime",  Tristate::Auto);
    useRawSsbo            = config.getOption<Tristate>("dxvk.useRawSsbo",             Tristate::Auto);
    hud                   = config.getOption<std::string>("dxvk.hud", "");
    tearFree              = config.getOption<Tristate>("dxvk.tearFree",               Tristate::Auto);
    hideIntegratedGraphics = config.getOption<bool>   ("dxvk.hideIntegratedGraphics", false);
    zeroMappedMemory      = config.getOption<bool>    ("dxvk.zeroMappedMemory",       false);
    zeroShaderScratchMemory = config.getOption<bool>  ("dxvk.zeroShaderScratchMemory", false);
    allowFse              = config.getOption<bool>    ("dxvk.allowFse",               false);
    deviceFilter          = config.getOption<std::string>("dxvk.deviceFilter",        "");
    lowerSinCos           = config.getOption<Tristate>("dxvk.lowerSinCos",            Tristate::Auto);
    tilerMode             = config.getOption<Tristate>("dxvk.tilerMode",              Tristate::Auto);

    auto budget = config.getOption<int32_t>("dxvk.maxMemoryBudget", 0);
    maxMemoryBudget = VkDeviceSize(std::max(budget, 0)) << 20u;

    // Mali-specific options (defaults are conservative; overridden in determineDeviceHints for Mali)
    enableINT8Shaders            = config.getOption<bool>("dxvk.enableINT8Shaders", false);
    enableDescriptorIndexing     = config.getOption<bool>("dxvk.enableDescriptorIndexing", false);
    enableBufferDeviceAddress    = config.getOption<bool>("dxvk.enableBufferDeviceAddress", false);
    enableShaderTileImage        = config.getOption<bool>("dxvk.enableShaderTileImage", false);
    enableARMBuiltins            = config.getOption<bool>("dxvk.enableARMBuiltins", false);
    enableSynchronization2       = config.getOption<bool>("dxvk.enableSynchronization2", false);
    enableTimelineSemaphore      = config.getOption<bool>("dxvk.enableTimelineSemaphore", false);
    preferCachedMemory           = config.getOption<Tristate>("dxvk.preferCachedMemory", Tristate::Auto);
    maxMemoryBudgetMB            = config.getOption<int32_t>("dxvk.maxMemoryBudget", 0);
  }

}
