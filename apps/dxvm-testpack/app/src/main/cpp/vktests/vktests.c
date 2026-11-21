/*
 * Vulkan capability suite for DXVM.
 *
 * These ask the driver the questions DXVM's feature table depends on: does it
 * sample BCn, does it do dual-source blending / logic ops / fill mode / more
 * than one viewport, and which of DXVK's required features are missing.
 *
 * They are capability queries, not rendering tests: they need no pipeline, and
 * they can run against any ICD including lavapipe on a build host. Results are
 * what the *driver* reports, which is what decides whether DXVM's emulation and
 * BCn remap are necessary on that device.
 *
 * Return codes: 0 pass, 1 fail, 2 skip.
 */
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define DETAIL_MAX 512

static VkInstance g_instance;
static VkPhysicalDevice g_physical;
static VkPhysicalDeviceProperties g_props;
static VkPhysicalDeviceFeatures g_feats;

static int init(char *detail) {
  VkApplicationInfo app = {0};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.apiVersion = VK_API_VERSION_1_1;

  VkInstanceCreateInfo ci = {0};
  ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ci.pApplicationInfo = &app;

  if (vkCreateInstance(&ci, NULL, &g_instance) != VK_SUCCESS) {
    /* A Vulkan 1.0-only driver refuses a 1.1 instance; retry before giving up. */
    app.apiVersion = VK_API_VERSION_1_0;
    if (vkCreateInstance(&ci, NULL, &g_instance) != VK_SUCCESS) {
      snprintf(detail, DETAIL_MAX, "cannot create a Vulkan instance");
      return 1;
    }
  }

  uint32_t count = 0;
  if (vkEnumeratePhysicalDevices(g_instance, &count, NULL) != VK_SUCCESS || count == 0) {
    snprintf(detail, DETAIL_MAX, "no Vulkan device");
    return 1;
  }
  VkPhysicalDevice devs[8];
  if (count > 8) count = 8;
  vkEnumeratePhysicalDevices(g_instance, &count, devs);
  g_physical = devs[0];

  vkGetPhysicalDeviceProperties(g_physical, &g_props);
  vkGetPhysicalDeviceFeatures(g_physical, &g_feats);
  return 0;
}

static int device_name(char *detail) {
  snprintf(detail, DETAIL_MAX, "%s (vendor 0x%04x, api %u.%u.%u)",
    g_props.deviceName, g_props.vendorID,
    VK_VERSION_MAJOR(g_props.apiVersion),
    VK_VERSION_MINOR(g_props.apiVersion),
    VK_VERSION_PATCH(g_props.apiVersion));
  return 0;
}

/* Does the driver support sampling compressed BCn images at all? Mali does not. */
static int test_bc(char *detail) {
  static const VkFormat bc[] = {
    VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_FORMAT_BC2_UNORM_BLOCK,
    VK_FORMAT_BC3_UNORM_BLOCK,      VK_FORMAT_BC4_UNORM_BLOCK,
    VK_FORMAT_BC5_UNORM_BLOCK,      VK_FORMAT_BC6H_UFLOAT_BLOCK,
    VK_FORMAT_BC7_UNORM_BLOCK,
  };
  int supported = 0;
  for (size_t i = 0; i < sizeof(bc) / sizeof(bc[0]); i++) {
    VkFormatProperties p;
    vkGetPhysicalDeviceFormatProperties(g_physical, bc[i], &p);
    if (p.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) supported++;
  }
  if (supported == 0) {
    snprintf(detail, DETAIL_MAX, "no BCn format is sampleable; DXVM must remap and transcode");
    return 1;
  }
  snprintf(detail, DETAIL_MAX, "%d of 7 BCn formats sampleable", supported);
  return 0;
}

static int test_blend_dual(char *detail) {
  if (!g_feats.dualSrcBlend) {
    snprintf(detail, DETAIL_MAX, "dual-source blending unsupported; DXVM clamps the blend factors");
    return 1;
  }
  snprintf(detail, DETAIL_MAX, "dual-source blending supported");
  return 0;
}

static int test_blend_logic(char *detail) {
  if (!g_feats.logicOp) {
    snprintf(detail, DETAIL_MAX, "logic ops unsupported; DXVM disables logicOpEnable");
    return 1;
  }
  snprintf(detail, DETAIL_MAX, "logic ops supported");
  return 0;
}

static int test_blend_mrt(char *detail) {
  const uint32_t need = 8;
  if (g_props.limits.maxColorAttachments < need) {
    snprintf(detail, DETAIL_MAX, "maxColorAttachments %u, need %u",
      g_props.limits.maxColorAttachments, need);
    return 1;
  }
  snprintf(detail, DETAIL_MAX, "maxColorAttachments %u",
    g_props.limits.maxColorAttachments);
  return 0;
}

static int test_fill_mode(char *detail) {
  if (!g_feats.fillModeNonSolid) {
    snprintf(detail, DETAIL_MAX, "polygonMode LINE/POINT unsupported; DXVM clamps to FILL");
    return 1;
  }
  snprintf(detail, DETAIL_MAX, "wireframe fill modes supported");
  return 0;
}

static int test_multi_viewport(char *detail) {
  if (!g_feats.multiViewport || g_props.limits.maxViewports < 2) {
    snprintf(detail, DETAIL_MAX, "maxViewports %u; DXVM clamps the count to 1",
      g_props.limits.maxViewports);
    return 1;
  }
  snprintf(detail, DETAIL_MAX, "maxViewports %u", g_props.limits.maxViewports);
  return 0;
}

/* The features DXVM's own feature table toggles, as the driver reports them. */
static int test_dxvk_reqs(char *detail) {
  struct { const char *name; VkBool32 on; } reqs[] = {
    { "dualSrcBlend",    g_feats.dualSrcBlend },
    { "fillModeNonSolid",g_feats.fillModeNonSolid },
    { "multiViewport",   g_feats.multiViewport },
    { "logicOp",         g_feats.logicOp },
    { "geometryShader",  g_feats.geometryShader },
    { "tessellationShader", g_feats.tessellationShader },
    { "shaderInt64",     g_feats.shaderInt64 },
    { "shaderFloat64",   g_feats.shaderFloat64 },
  };
  char missing[320] = {0};
  int n = 0;
  for (size_t i = 0; i < sizeof(reqs) / sizeof(reqs[0]); i++) {
    if (!reqs[i].on) {
      if (n++) strncat(missing, ", ", sizeof(missing) - strlen(missing) - 1);
      strncat(missing, reqs[i].name, sizeof(missing) - strlen(missing) - 1);
    }
  }
  if (n == 0) {
    snprintf(detail, DETAIL_MAX, "all %zu checked features present", sizeof(reqs) / sizeof(reqs[0]));
    return 0;
  }
  snprintf(detail, DETAIL_MAX, "missing: %s", missing);
  return 1;
}

typedef struct { const char *name; int (*fn)(char *); } Test;

static const Test TESTS[] = {
  { "vk.device",          device_name },
  { "vk.bc",              test_bc },
  { "vk.blend-dual",      test_blend_dual },
  { "vk.blend-logic",     test_blend_logic },
  { "vk.blend-mrt",       test_blend_mrt },
  { "vk.fill-mode",       test_fill_mode },
  { "vk.multi-viewport",  test_multi_viewport },
  { "vk.dxvk-reqs",       test_dxvk_reqs },
};
#define NTESTS (sizeof(TESTS) / sizeof(TESTS[0]))

/* JNI-facing entry: runs one named test, returns 0/1/2 and fills detail. */
int vktests_run(const char *name, char *detail, int detail_len) {
  char buf[DETAIL_MAX] = {0};
  int rc = 2;
  snprintf(buf, sizeof(buf), "driver suite not initialised");

  if (init(buf) == 0) {
    for (size_t i = 0; i < NTESTS; i++) {
      if (strcmp(TESTS[i].name, name) == 0) { rc = TESTS[i].fn(buf); break; }
    }
    if (rc == 2 && buf[0] == 'd') snprintf(buf, sizeof(buf), "unknown test: %s", name);
  }
  if (detail && detail_len > 0) {
    snprintf(detail, (size_t)detail_len, "%s", buf);
  }
  if (g_instance) { vkDestroyInstance(g_instance, NULL); g_instance = VK_NULL_HANDLE; }
  return rc;
}

/* ---- identity of the device under test ------------------------------------
 *
 * The report's headline comes from here: which driver actually answered. The
 * KOTLIN side decides what that identity *means* (system vs PanVK); this only
 * reports what Vulkan says, so a wrong verdict is a Kotlin bug, not a C one.
 *
 * Output shape (single line, '|'-separated, no '|' inside the values):
 *   vendorId|apiVersion|driverId|deviceName|driverName|driverInfo|driverVersion
 * driverId is the VK_DRIVER_ID_* spelling where known, else the numeric value.
 */
#define IDENTITY_MAX 1024

static void copy_field(char *dst, size_t dst_len, const char *src) {
  if (!dst_len) return;
  size_t n = 0;
  while (src && src[n] && n + 1 < dst_len) {
    /* '|' and newlines would break the wire format. */
    dst[n] = (src[n] == '|' || src[n] == '\n' || src[n] == '\r') ? ' ' : src[n];
    n++;
  }
  dst[n] = 0;
}

static const char *driver_id_name(VkDriverId id) {
  switch (id) {
    case VK_DRIVER_ID_ARM_PROPRIETARY:         return "VK_DRIVER_ID_ARM_PROPRIETARY";
    case VK_DRIVER_ID_MESA_PANVK:              return "VK_DRIVER_ID_MESA_PANVK";
    case VK_DRIVER_ID_MESA_LLVMPIPE:           return "VK_DRIVER_ID_MESA_LLVMPIPE";
    case VK_DRIVER_ID_MESA_TURNIP:             return "VK_DRIVER_ID_MESA_TURNIP";
    case VK_DRIVER_ID_MESA_RADV:               return "VK_DRIVER_ID_MESA_RADV";
    case VK_DRIVER_ID_MESA_VENUS:              return "VK_DRIVER_ID_MESA_VENUS";
    case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA:  return "VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA";
    case VK_DRIVER_ID_QUALCOMM_PROPRIETARY:    return "VK_DRIVER_ID_QUALCOMM_PROPRIETARY";
    case VK_DRIVER_ID_IMAGINATION_PROPRIETARY: return "VK_DRIVER_ID_IMAGINATION_PROPRIETARY";
    default:                                   return "";
  }
}

/* Fills `out` with the identity line. Returns 0 when a device was read. */
int vktests_identity(char *out, int out_len) {
  char err[DETAIL_MAX] = {0};
  char driver_id[64] = "";
  char device_name[256] = "";
  char driver_name[256] = "";
  char driver_info[256] = "";
  unsigned vendor = 0, api = 0;
  unsigned long long driver_version = 0;

  if (out == NULL || out_len <= 0) return 1;
  if (init(err) != 0) {
    snprintf(out, (size_t)out_len, "0|0||%s|||0", err);
    return 1;
  }

  vendor = g_props.vendorID;
  api = g_props.apiVersion;
  driver_version = (unsigned long long)g_props.driverVersion;
  copy_field(device_name, sizeof(device_name), g_props.deviceName);

  /* VK_EXT_driver_properties / Vulkan 1.1+: the driver's own id and version string. */
  if (api >= VK_API_VERSION_1_1) {
    VkPhysicalDeviceDriverProperties dp;
    VkPhysicalDeviceProperties2 p2;
    memset(&dp, 0, sizeof(dp));
    memset(&p2, 0, sizeof(p2));
    dp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
    p2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    p2.pNext = &dp;
    vkGetPhysicalDeviceProperties2(g_physical, &p2);
    copy_field(driver_id, sizeof(driver_id), driver_id_name(dp.driverID));
    if (driver_id[0] == 0) snprintf(driver_id, sizeof(driver_id), "%d", (int)dp.driverID);
    copy_field(driver_name, sizeof(driver_name), dp.driverName);
    copy_field(driver_info, sizeof(driver_info), dp.driverInfo);
  }

  snprintf(out, (size_t)out_len, "%u|%u|%s|%s|%s|%s|%llu",
    vendor, api, driver_id, device_name, driver_name, driver_info, driver_version);

  if (g_instance) { vkDestroyInstance(g_instance, NULL); g_instance = VK_NULL_HANDLE; }
  return 0;
}

/* Test inventory, so the app cannot drift from the suites the host script runs. */
int vktests_count(void) { return (int)NTESTS; }

const char *vktests_name(int i) {
  return (i >= 0 && i < (int)NTESTS) ? TESTS[i].name : NULL;
}

/* Host build entry: run everything and print DXML-shaped lines. */
#ifndef VKTESTS_NO_MAIN
int main(void) {
  char detail[DETAIL_MAX];
  int passed = 0, failed = 0, skipped = 0;
  for (size_t i = 0; i < NTESTS; i++) {
    int rc = vktests_run(TESTS[i].name, detail, sizeof(detail));
    const char *kind = rc == 0 ? "PASS" : rc == 1 ? "FAIL" : "SKIP";
    if (rc == 0) passed++; else if (rc == 1) failed++; else skipped++;
    /* name padded to column 15, minimum one space */
    int pad = (int)strlen(TESTS[i].name) >= 15 ? 1 : 15 - (int)strlen(TESTS[i].name);
    printf("%s %s%*s%s\n", kind, TESTS[i].name, pad, "", detail);
  }
  printf("SUMMARY %d passed, %d failed, %d skipped\n", passed, failed, skipped);
  return failed ? 1 : 0;
}
#endif
