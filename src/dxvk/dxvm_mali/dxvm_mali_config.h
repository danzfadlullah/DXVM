#pragma once

#include "../../util/config/config.h"

namespace dxvk {

  /**
   * \brief Mali configuration, parsed once from \c dxvk.* options
   *
   * All 12 Mali-specific options are represented. Defaults mirror the
   * fallbacks in DxvkOptions, except for \c enableDynamicRendering, which
   * defaults to true for Mali as required by the approved design.
   */
  struct DxvmMaliConfig {
    explicit DxvmMaliConfig(const Config& config);

    /// Enable dynamic rendering (VK_KHR_dynamic_rendering)
    bool      enableDynamicRendering   = true;
    /// Enable FP16 shader arithmetic (VK_KHR_shader_float16_int8)
    bool      enableFP16Shaders        = false;
    /// Enable INT8 shader arithmetic (VK_KHR_shader_float16_int8)
    bool      enableINT8Shaders        = false;
    /// Enable descriptor indexing (VK_EXT_descriptor_indexing)
    bool      enableDescriptorIndexing = false;
    /// Enable buffer device address (VK_KHR_buffer_device_address)
    bool      enableBufferDeviceAddress = false;
    /// Enable shader tile image (VK_EXT_shader_tile_image)
    bool      enableShaderTileImage    = false;
    /// Enable ARM shader core builtins (VK_ARM_shader_core_builtins)
    bool      enableARMBuiltins        = false;
    /// Enable synchronization2 (VK_KHR_synchronization2)
    bool      enableSynchronization2   = false;
    /// Enable timeline semaphores (VK_KHR_timeline_semaphore)
    bool      enableTimelineSemaphore  = false;
    /// Prefer cached memory for Mali unified memory
    Tristate  preferCachedMemory       = Tristate::Auto;
    /// Maximum memory budget in MB (for unified memory systems)
    int32_t   maxMemoryBudgetMB        = 0;
    /// Enable BCn texture format transcoding to ASTC/ETC2 (Mali GPUs)
    bool      enableBcnTranscoding     = true;
  };

}
