#include "dxvk/dxvm_mali/dxvm_mali_state_emulation.h"

#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (false)

int main() {
  using namespace dxvk;

  VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.polygonMode = VK_POLYGON_MODE_LINE;
  VkPipelineColorBlendAttachmentState att{};
  att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA;
  att.blendEnable = VK_TRUE;
  VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  cb.logicOpEnable = VK_TRUE;
  cb.attachmentCount = 1;
  cb.pAttachments = &att;
  VkViewport viewport{};
  VkPipelineViewportStateCreateInfo vs{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  vs.viewportCount = 4;
  vs.scissorCount = 4;
  vs.pViewports = &viewport;
  VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  gp.pRasterizationState = &rs;
  gp.pColorBlendState = &cb;
  gp.pViewportState = &vs;

  DxvmMaliEmulatedState out;

  // Non-Mali: no changes captured.
  dxvmMaliEmulateGraphicsState(false, gp, out);
  CHECK(!out.hasRaster && !out.hasBlend);

  // Mali: clamped local copies.
  dxvmMaliEmulateGraphicsState(true, gp, out);
  CHECK(out.hasRaster && out.rsInfo.polygonMode == VK_POLYGON_MODE_FILL);
  CHECK(out.cbInfo.logicOpEnable == VK_FALSE);
  CHECK(out.cbInfo.pAttachments[0].srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA);
  CHECK(out.viewportCount == 1);
  CHECK(out.vpInfo.viewportCount == 1);
  CHECK(out.vpInfo.scissorCount <= 1);
  CHECK(out.vpInfo.pViewports != nullptr);
  CHECK(out.vpInfo.pScissors == &out.scissor);

  if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
  std::printf("test_emulation: OK\n");
  return 0;
}
