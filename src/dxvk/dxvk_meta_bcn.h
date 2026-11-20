#pragma once

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <string>

#include "dxvk_format.h"
#include "dxvk_pipelayout.h"

namespace dxvk {

  class DxvkDevice;

  struct DxvkMetaBcnTranscodePipeline {
    const DxvkPipelineLayout* layout = nullptr;
    VkPipeline pipeline = VK_NULL_HANDLE;
  };

  class DxvkMetaBcnTranscodeObjects {
  public:

    DxvkMetaBcnTranscodeObjects(DxvkDevice* device);
    ~DxvkMetaBcnTranscodeObjects();

    // Get pipeline for BCn format
    DxvkMetaBcnTranscodePipeline getPipeline(VkFormat bcnFormat);

    // Check if format is supported
    bool isFormatSupported(VkFormat bcnFormat) const;

  private:

    DxvkDevice* m_device = nullptr;

    const DxvkPipelineLayout* m_layout = nullptr;
    std::unordered_map<VkFormat, DxvkMetaBcnTranscodePipeline> m_pipelines;
    std::unordered_set<VkFormat> m_supportedFormats;

    const DxvkPipelineLayout* createPipelineLayout() const;

    DxvkMetaBcnTranscodePipeline createPipeline(VkFormat bcnFormat);
    std::string getShaderName(VkFormat bcnFormat) const;
  };

}