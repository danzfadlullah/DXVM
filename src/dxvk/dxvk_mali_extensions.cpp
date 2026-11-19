#include "dxvk_mali_extensions.h"
#include "dxvk_instance.h"
#include "dxvk_adapter.h"

namespace dxvk {

  DxvkMaliExtensionProvider DxvkMaliExtensionProvider::s_instance = DxvkMaliExtensionProvider();

  DxvkMaliExtensionProvider::DxvkMaliExtensionProvider(const DxvkInstance* instance)
  : m_instance(instance) {
  }

  DxvkExtensionList DxvkMaliExtensionProvider::getInstanceExtensions() {
    DxvkExtensionList extList;

    // Instance extensions for Mali ARM
    extList.push_back(vk::makeExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME));

    return extList;
  }

  DxvkExtensionList DxvkMaliExtensionProvider::getDeviceExtensions(uint32_t adapterId) {
    DxvkExtensionList extList;

    if (adapterId >= m_numAdapters)
      return extList;

    const auto& adapter = m_instance->enumAdapters(adapterId);
    if (!adapter || !adapter->isMali())
      return extList;

    // ARM Mali-specific device extensions
    if (adapter->hasMaliExtension(VK_ARM_SHADER_CORE_BUILTINS_EXTENSION_NAME))
      extList.push_back(vk::makeExtension(VK_ARM_SHADER_CORE_BUILTINS_EXTENSION_NAME));

    if (adapter->hasMaliExtension(VK_EXT_SHADER_TILE_IMAGE_EXTENSION_NAME))
      extList.push_back(vk::makeExtension(VK_EXT_SHADER_TILE_IMAGE_EXTENSION_NAME));

    if (adapter->hasMaliExtension(VK_ARM_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME))
      extList.push_back(vk::makeExtension(VK_ARM_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME));

    if (adapter->hasMaliExtension(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME))
      extList.push_back(vk::makeExtension(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME));

    return extList;
  }

  void DxvkMaliExtensionProvider::initInstanceExtensions() {
    // No special instance extensions needed for ARM Mali
  }

  void DxvkMaliExtensionProvider::initDeviceExtensions(const DxvkInstance* instance) {
    m_instance = instance;
    m_numAdapters = 0;

    // Count Mali adapters
    for (uint32_t i = 0; i < 16; i++) {
      if (instance->enumAdapters(i)) {
        m_numAdapters++;
      } else {
        break;
      }
    }
  }

}