#pragma once

#include "dxvk_extension_provider.h"

namespace dxvk {

  /**
   * \brief Mali ARM extension provider
   *
   * Provides ARM-specific Vulkan extensions for Mali GPUs:
   * - VK_ARM_shader_core_builtins
   * - VK_EXT_shader_tile_image
   * - VK_ARM_rasterization_order_attachment_access
   * - VK_KHR_fragment_shading_rate
   */
  class DxvkMaliExtensionProvider : public DxvkExtensionProvider {

  public:

    static DxvkMaliExtensionProvider s_instance;

    DxvkMaliExtensionProvider(const DxvkInstance* instance = nullptr);

    std::string_view getName() override {
      return "Mali ARM Extensions";
    }

    DxvkExtensionList getInstanceExtensions() override;

    DxvkExtensionList getDeviceExtensions(uint32_t adapterId) override;

    void initInstanceExtensions() override;

    void initDeviceExtensions(const DxvkInstance* instance) override;

  private:

    const DxvkInstance* m_instance = nullptr;
    uint32_t m_numAdapters = 0;

  };

}