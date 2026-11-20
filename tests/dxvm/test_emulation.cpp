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
  VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  gp.pRasterizationState = &rs;
  gp.pColorBlendState = &cb;
  uint32_t count = 1, vp = 4;

  // Non-Mali: no changes.
  dxvmMaliEmulateGraphicsState(false, gp, &att, count, vp);
  CHECK(rs.polygonMode == VK_POLYGON_MODE_LINE);
  CHECK(vp == 4);

  // Mali: clamp everything.
  dxvmMaliEmulateGraphicsState(true, gp, &att, count, vp);
  CHECK(rs.polygonMode == VK_POLYGON_MODE_FILL);
  CHECK(att.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA);
  CHECK(vp == 1);
  CHECK(cb.logicOpEnable == VK_FALSE);

  if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
  std::printf("test_emulation: OK\n");
  return 0;
}
