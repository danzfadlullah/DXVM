#include "dxvk_meta_bcn.h"
#include "dxvk_device.h"
#include "dxvk_shader.h"
#include "dxvk_format.h"
#include "dxvk_shader_builtin.h"

// Include generated shader headers
#include "bcn_transcode_bc1.h"
#include "bcn_transcode_bc3.h"
#include "bcn_transcode_bc4_bc5.h"
#include "bcn_transcode_bc6h.h"
#include "bcn_transcode_bc7.h"

namespace dxvk {

  DxvkMetaBcnTranscodeObjects::DxvkMetaBcnTranscodeObjects(DxvkDevice* device)
  : m_device(device), m_layout(createPipelineLayout()) {
    // Initialize supported formats
    m_supportedFormats = {
      VK_FORMAT_BC1_RGB_UNORM_BLOCK,
      VK_FORMAT_BC1_RGB_SRGB_BLOCK,
      VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
      VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
      VK_FORMAT_BC2_UNORM_BLOCK,
      VK_FORMAT_BC2_SRGB_BLOCK,
      VK_FORMAT_BC3_UNORM_BLOCK,
      VK_FORMAT_BC3_SRGB_BLOCK,
      VK_FORMAT_BC4_UNORM_BLOCK,
      VK_FORMAT_BC4_SNORM_BLOCK,
      VK_FORMAT_BC5_UNORM_BLOCK,
      VK_FORMAT_BC5_SNORM_BLOCK,
      VK_FORMAT_BC6H_UFLOAT_BLOCK,
      VK_FORMAT_BC6H_SFLOAT_BLOCK,
      VK_FORMAT_BC7_UNORM_BLOCK,
      VK_FORMAT_BC7_SRGB_BLOCK,
    };
  }

  DxvkMetaBcnTranscodeObjects::~DxvkMetaBcnTranscodeObjects() {
    for (auto& [format, pipeline] : m_pipelines) {
      if (pipeline.pipeline)
        m_device->vkd()->vkDestroyPipeline(m_device->vkd()->device(), pipeline.pipeline, nullptr);
    }
  }

  std::string DxvkMetaBcnTranscodeObjects::getShaderName(VkFormat bcnFormat) const {
    switch (bcnFormat) {
      case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
      case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
      case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
      case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        return "bcn_transcode_bc1";
      case VK_FORMAT_BC2_UNORM_BLOCK:
      case VK_FORMAT_BC2_SRGB_BLOCK:
      case VK_FORMAT_BC3_UNORM_BLOCK:
      case VK_FORMAT_BC3_SRGB_BLOCK:
        return "bcn_transcode_bc3";
      case VK_FORMAT_BC4_UNORM_BLOCK:
      case VK_FORMAT_BC4_SNORM_BLOCK:
      case VK_FORMAT_BC5_UNORM_BLOCK:
      case VK_FORMAT_BC5_SNORM_BLOCK:
        return "bcn_transcode_bc4_bc5";
      case VK_FORMAT_BC6H_UFLOAT_BLOCK:
      case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        return "bcn_transcode_bc6h";
      case VK_FORMAT_BC7_UNORM_BLOCK:
      case VK_FORMAT_BC7_SRGB_BLOCK:
        return "bcn_transcode_bc7";
      default:
        return "";
    }
  }

  bool DxvkMetaBcnTranscodeObjects::isFormatSupported(VkFormat bcnFormat) const {
    return m_supportedFormats.find(bcnFormat) != m_supportedFormats.end();
  }

  DxvkMetaBcnTranscodePipeline DxvkMetaBcnTranscodeObjects::getPipeline(VkFormat bcnFormat) {
    auto it = m_pipelines.find(bcnFormat);
    if (it != m_pipelines.end())
      return it->second;

    if (!isFormatSupported(bcnFormat))
      return {};

    DxvkMetaBcnTranscodePipeline pipeline = createPipeline(bcnFormat);
    m_pipelines.emplace(bcnFormat, pipeline);
    return pipeline;
  }

  DxvkMetaBcnTranscodePipeline DxvkMetaBcnTranscodeObjects::createPipeline(VkFormat bcnFormat) {
    // Get the shader code array for this format
    const uint32_t* shaderCode = nullptr;
    size_t shaderSize = 0;

    if (bcnFormat == VK_FORMAT_BC1_RGB_UNORM_BLOCK || bcnFormat == VK_FORMAT_BC1_RGB_SRGB_BLOCK ||
        bcnFormat == VK_FORMAT_BC1_RGBA_UNORM_BLOCK || bcnFormat == VK_FORMAT_BC1_RGBA_SRGB_BLOCK) {
      shaderCode = bcn_transcode_bc1;
      shaderSize = sizeof(bcn_transcode_bc1);
    } else if (bcnFormat == VK_FORMAT_BC2_UNORM_BLOCK || bcnFormat == VK_FORMAT_BC2_SRGB_BLOCK ||
               bcnFormat == VK_FORMAT_BC3_UNORM_BLOCK || bcnFormat == VK_FORMAT_BC3_SRGB_BLOCK) {
      shaderCode = bcn_transcode_bc3;
      shaderSize = sizeof(bcn_transcode_bc3);
    } else if (bcnFormat == VK_FORMAT_BC4_UNORM_BLOCK || bcnFormat == VK_FORMAT_BC4_SNORM_BLOCK ||
               bcnFormat == VK_FORMAT_BC5_UNORM_BLOCK || bcnFormat == VK_FORMAT_BC5_SNORM_BLOCK) {
      shaderCode = bcn_transcode_bc4_bc5;
      shaderSize = sizeof(bcn_transcode_bc4_bc5);
    } else if (bcnFormat == VK_FORMAT_BC6H_UFLOAT_BLOCK || bcnFormat == VK_FORMAT_BC6H_SFLOAT_BLOCK) {
      shaderCode = bcn_transcode_bc6h;
      shaderSize = sizeof(bcn_transcode_bc6h);
    } else if (bcnFormat == VK_FORMAT_BC7_UNORM_BLOCK || bcnFormat == VK_FORMAT_BC7_SRGB_BLOCK) {
      shaderCode = bcn_transcode_bc7;
      shaderSize = sizeof(bcn_transcode_bc7);
    } else {
      return {};
    }

    // Create shader stage from code array
    std::vector<uint32_t> shaderWords(shaderCode, shaderCode + (shaderSize / sizeof(uint32_t)));
    util::DxvkBuiltInShaderStage shader(shaderWords);

    VkPipeline pipeline = m_device->createBuiltInComputePipeline(m_layout, shader);

    DxvkMetaBcnTranscodePipeline result;
    result.layout = m_layout;
    result.pipeline = pipeline;
    return result;
  }

  const DxvkPipelineLayout* DxvkMetaBcnTranscodeObjects::createPipelineLayout() const {
    std::array<DxvkDescriptorSetLayoutBinding, 2> bindings = {{
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT },
    }};

    return m_device->createBuiltInPipelineLayout(DxvkPipelineLayoutFlag::UsesSamplerHeap,
      VK_SHADER_STAGE_COMPUTE_BIT, sizeof(uint32_t) * 2, bindings.size(), bindings.data());
  }

}