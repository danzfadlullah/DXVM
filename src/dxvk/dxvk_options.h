#pragma once

#include "../util/config/config.h"
#include "../util/util_env.h"

#include "../vulkan/vulkan_loader.h"

namespace dxvk {

  struct DxvkOptions {
    DxvkOptions() { }
    DxvkOptions(const Config& config);

    /// Enable debug utils
    bool enableDebugUtils = false;

    /// Enable memory defragmentation
    Tristate enableMemoryDefrag = Tristate::Auto;

    /// Number of compiler threads
    /// when using the state cache
    int32_t numCompilerThreads = 0;

    /// Enable graphics pipeline library
    Tristate enableGraphicsPipelineLibrary = Tristate::Auto;

    /// Enable descriptor heap
    Tristate enableDescriptorHeap = Tristate::Auto;

    /// Enable descriptor buffer
    Tristate enableDescriptorBuffer = Tristate::Auto;

    /// Enable unified image layout path
    bool enableUnifiedImageLayout = true;

    /// Enables pipeline lifetime tracking
    Tristate trackPipelineLifetime = Tristate::Auto;

    /// Shader-related options
    Tristate useRawSsbo = Tristate::Auto;

    /// HUD elements
    std::string hud;

    /// Forces swap chain into MAILBOX (if true)
    /// or FIFO_RELAXED (if false) present mode
    Tristate tearFree = Tristate::Auto;

    /// Disable VK_NV_low_latency2. This extension
    /// appears to be all sorts of broken on 32-bit.
    Tristate enableNvLowLatency2 = Tristate::Auto;

    // Hides integrated GPUs if dedicated GPUs are
    // present. May be necessary for some games that
    // incorrectly assume monitor layouts.
    bool hideIntegratedGraphics = false;

    /// Clears all mapped memory to zero.
    bool zeroMappedMemory = false;

    /// Allows full-screen exclusive mode on Windows
    bool allowFse = false;

    /// Whether to enable tiler optimizations
    Tristate tilerMode = Tristate::Auto;

    /// Overrides memory budget for DXVK
    VkDeviceSize maxMemoryBudget = 0u;

    /// Whether to use custom sin/cos approximation
    Tristate lowerSinCos = Tristate::Auto;

    /// Enables implicit resolves that are used to
    /// deal with MSAA-related undefined behaviour.
    bool enableImplicitResolves = true;

    /// Enables NV_raw_access_chains extension on Nvidia
    bool enableNvRawAccessChains = true;

    /// Enables CUDA interop extensions if available
    bool enableNvCudaInterop = true;

    /// Enable present timing features
    bool enablePresentTiming = true;

    /// Enable descriptor update templates
    bool enableDescriptorUpdateTemplates = env::is32BitHostPlatform();

    /// Whether scratch memory in shaders needs
    /// zero-initialization to work around game bugs.
    bool zeroShaderScratchMemory = false;

    /// Device name
    std::string deviceFilter;

    // Mali-specific options
    /// Enable dynamic rendering (VK_KHR_dynamic_rendering)
    bool enableDynamicRendering = true;
    /// Enable FP16 shader arithmetic (VK_KHR_shader_float16_int8)
    bool enableFP16Shaders = true;
    /// Enable INT8 shader arithmetic (VK_KHR_shader_float16_int8)
    bool enableINT8Shaders = true;
    /// Enable descriptor indexing (VK_EXT_descriptor_indexing)
    bool enableDescriptorIndexing = true;
    /// Enable buffer device address (VK_KHR_buffer_device_address)
    bool enableBufferDeviceAddress = true;
    /// Enable shader tile image (VK_EXT_shader_tile_image)
    bool enableShaderTileImage = true;
    /// Enable ARM shader core builtins (VK_ARM_shader_core_builtins)
    bool enableARMBuiltins = true;
    /// Enable synchronization2 (VK_KHR_synchronization2)
    bool enableSynchronization2 = true;
    /// Enable timeline semaphores (VK_KHR_timeline_semaphore)
    bool enableTimelineSemaphore = true;
    /// Prefer cached memory for Mali unified memory
    Tristate preferCachedMemory = Tristate::Auto;
    /// Maximum memory budget in MB (for unified memory systems)
    int32_t maxMemoryBudgetMB = 3072;

    /// Enable BCn texture format transcoding to ASTC/ETC2 (Mali GPUs)
    bool enableBcnTranscoding = true;
  };

}
