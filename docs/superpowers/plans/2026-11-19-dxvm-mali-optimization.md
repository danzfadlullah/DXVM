# DXVM Mali-G615 Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement comprehensive Mali GPU optimizations in DXVM (DXVK fork) for Vulkan 1.3+ Mali GPUs (Valhall/Bifrost), using Mali-G615 MC2 as the reference development device.

**Architecture:** Feature-gated optimizations detected at runtime via `VK_DRIVER_ID_ARM_PROPRIETARY` and device properties. All optimizations are conditional and fallback gracefully. Changes are minimally invasive to DXVK upstream merge compatibility.

**Tech Stack:** C++17, Meson build, MinGW-w64 cross-compile, Vulkan 1.3, SPIR-V, glslang, DXBC→SPIRV translator (dxbc-spirv)

**Spec:** `docs/superpowers/specs/2026-11-19-dxvm-mali-optimization-design.md`

---

## Global Constraints

| Constraint | Value |
|------------|-------|
| Minimum Vulkan API | 1.3 (VK_API_VERSION_1_3) |
| Target GPU Architecture | ARM Mali Valhall (G615/G715/G720) & Bifrost (G52/G57/G76/G77) |
| Reference Device | Mali-G615 MC2 (vendor 0x13B5, driver v1.r44p1+) |
| Platform | Android 10+ (API 29+) via Termux/proot, also Linux native |
| Build Target | Windows DLL (x86_64/i686 via MinGW-w64) + Linux .so (aarch64) |
| Memory Model | Unified memory (system RAM = VRAM), budget configurable |
| Upstream Compatibility | Must not break non-Mali GPU paths |
| C++ Standard | C++17 (Meson `cpp_std=c++17`) |
| Driver ID Check | `VK_DRIVER_ID_ARM_PROPRIETARY` (primary), `VK_DRIVER_ID_MESA_PANVK` (Mesa) |

---

## Review Focus

| Input Class / Failure Mode | Expected Behavior | Owner Task |
|----------------------------|-------------------|------------|
| Non-Mali GPU (NVIDIA/AMD/Intel) | All Mali optimizations disabled, fallback to standard DXVK paths | Task 1 |
| Mali driver without specific extension (e.g., no `VK_EXT_shader_tile_image`) | Feature gated off, no crash, log warning | Task 5 |
| Unified memory pressure (OOM) | Memory defrag triggers, budget respected, graceful degradation | Task 4 |
| Shader compile stutter in game | GPL pre-compilation + async pipeline + persistent cache keeps stutter < 16ms | Task 3 |
| Box64/Wine loading DXVM DLL | DLL exports correct D3D8/9/10/11/DXGI entry points, no symbol conflicts | Task 7 |

---

## File Structure Map

```
src/
├── dxvk/
│   ├── dxvk_adapter.cpp/h          ← Task 1: Mali detection
│   ├── dxvk_device.cpp/h           ← Task 2,4,6: Hints, memory, sync
│   ├── dxvk_options.cpp/h          ← Task 1: Config options
│   ├── dxvk_instance.cpp/h         ← Task 1: Adapter enumeration
│   ├── dxvk_shader_spirv.cpp/h     ← Task 3: SPIR-V generation
│   ├── dxvk_shader_ir.cpp/h        ← Task 3: IR optimization
│   ├── dxvk_pipemanager.cpp/h      ← Task 3: GPL + pipeline cache
│   ├── dxvk_descriptor_pool.cpp/h  ← Task 4: Descriptor indexing
│   ├── dxvk_memory.cpp/h           ← Task 4: Unified memory
│   ├── dxvk_buffer.cpp/h           ← Task 4: BDA
│   ├── dxvk_barrier.cpp/h          ← Task 6: Synchronization2
│   ├── dxvk_renderpass.cpp/h       ← Task 2: Dynamic rendering
│   ├── dxvk_graphics.cpp/h         ← Task 2: Pipeline state
│   ├── dxvk_cmdlist.cpp/h          ← Task 2,6: Command recording
│   ├── dxvk_format.cpp/h           ← Task 4: Format support
│   └── dxvk_shader_cache.cpp/h     ← Task 3: Shader cache
├── d3d11/
│   └── d3d11_device.cpp            ← Task 8: Vendor hacks
├── d3d9/
│   └── d3d9_adapter.cpp            ← Task 8: Vendor reporting
└── vulkan/
    └── vulkan_loader.cpp           ← Task 7: Android WSI
```

---

## Task 1: Mali Device Detection & Configuration Options

**Files:**
- Modify: `src/dxvk/dxvk_adapter.cpp:60-90` (add `isMali()` method)
- Modify: `src/dxvk/dxvk_adapter.h` (add Mali detection declarations)
- Modify: `src/dxvk/dxvk_device.cpp:690-730` (use Mali detection for hints)
- Modify: `src/dxvk/dxvk_options.cpp/h` (add Mali-specific config options)
- Modify: `src/dxvk/dxvk_instance.cpp` (log Mali detection)

**Interfaces:**
- Consumes: `VkPhysicalDeviceProperties2`, `VkPhysicalDeviceVulkan12Properties::driverID`
- Produces: `DxvkAdapter::isMali()`, `DxvkAdapter::getMaliArchitecture()`, `DxvkOptions` Mali fields

- [ ] **Step 1: Write failing test for Mali detection**

```cpp
// tests/dxvk/test_mali_detection.cpp
TEST(MaliDetection, DetectsARMProprietaryDriver) {
  // Mock VkPhysicalDeviceProperties2 with driverID = VK_DRIVER_ID_ARM_PROPRIETARY
  // Verify DxvkAdapter::isMali() returns true
}
TEST(MaliDetection, DetectsPanVKDriver) {
  // Mock with driverID = VK_DRIVER_ID_MESA_PANVK
  // Verify isMali() returns true
}
TEST(MaliDetection, RejectsNonMaliDrivers) {
  // Mock with NVIDIA/AMD/Intel driver IDs
  // Verify isMali() returns false
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `meson test -C build test_mali_detection`
Expected: FAIL (test file doesn't exist, methods not implemented)

- [ ] **Step 3: Implement Mali detection in `src/dxvk/dxvk_adapter.h/cpp`**

```cpp
// dxvk_adapter.h
class DxvkAdapter {
public:
  bool isMali() const;
  enum class MaliArchitecture { Unknown, Bifrost, Valhall };
  MaliArchitecture getMaliArchitecture() const;
  bool hasMaliExtension(const char* extName) const;
private:
  MaliArchitecture m_maliArch = MaliArchitecture::Unknown;
};

// dxvk_adapter.cpp - in constructor after m_capabilities init:
const auto& props = m_capabilities.getProperties();
m_isMali = props.vk12.driverID == VK_DRIVER_ID_ARM_PROPRIETARY
        || props.vk12.driverID == VK_DRIVER_ID_MESA_PANVK;
if (m_isMali) {
  // Detect architecture from device name or driver info
  std::string devName(props.core.properties.deviceName);
  if (devName.find("G61") != std::string::npos ||  // G610, G615
      devName.find("G71") != std::string::npos ||  // G710, G715
      devName.find("G72") != std::string::npos)    // G720
    m_maliArch = MaliArchitecture::Valhall;
  else if (devName.find("G5") != std::string::npos ||  // G52, G57
           devName.find("G7") != std::string::npos)    // G76, G77
    m_maliArch = MaliArchitecture::Bifrost;
}
```

- [ ] **Step 4: Add Mali config options in `src/dxvk/dxvk_options.h/cpp`**

```cpp
// dxvk_options.h - add to DxvkOptions struct:
bool enableDynamicRendering = true;
bool enableFP16Shaders = true;
bool enableINT8Shaders = true;
bool enableDescriptorIndexing = true;
bool enableBufferDeviceAddress = true;
bool enableShaderTileImage = true;
bool enableARMBuiltins = true;
bool enableSynchronization2 = true;
bool enableTimelineSemaphore = true;
bool enableGraphicsPipelineLibrary = true;
Tristate tilerMode = Tristate::Auto;
Tristate preferCachedMemory = Tristate::Auto;
int32_t maxMemoryBudgetMB = 3072;

// dxvk_options.cpp - in constructor:
enableDynamicRendering = config.getOption<Tristate>("dxvk.enableDynamicRendering", isMali ? Tristate::True : Tristate::Auto);
enableFP16Shaders = config.getOption<bool>("dxvk.enableFP16Shaders", isMali);
enableINT8Shaders = config.getOption<bool>("dxvk.enableINT8Shaders", isMali);
enableDescriptorIndexing = config.getOption<bool>("dxvk.enableDescriptorIndexing", isMali);
enableBufferDeviceAddress = config.getOption<bool>("dxvk.enableBufferDeviceAddress", isMali);
enableShaderTileImage = config.getOption<bool>("dxvk.enableShaderTileImage", isMali && hasShaderTileImage);
enableARMBuiltins = config.getOption<bool>("dxvk.enableARMBuiltins", isMali && hasARMBuiltins);
enableSynchronization2 = config.getOption<bool>("dxvk.enableSynchronization2", isMali);
enableTimelineSemaphore = config.getOption<bool>("dxvk.enableTimelineSemaphore", isMali);
enableGraphicsPipelineLibrary = config.getOption<Tristate>("dxvk.enableGraphicsPipelineLibrary", isMali ? Tristate::True : Tristate::Auto);
tilerMode = config.getOption<Tristate>("dxvk.tilerMode", isMali ? Tristate::True : Tristate::Auto);
preferCachedMemory = config.getOption<Tristate>("dxvk.preferCachedMemory", isMali ? Tristate::True : Tristate::Auto);
maxMemoryBudgetMB = config.getOption<int32_t>("dxvk.maxMemoryBudget", isMali ? 3072 : 0);
```

- [ ] **Step 5: Update device hints in `src/dxvk/dxvk_device.cpp:690-730`**

```cpp
// In determineDeviceHints():
bool isMali = m_adapter->isMali();
auto maliArch = m_adapter->getMaliArchitecture();

// Tiler mode for all Mali
bool tilerMode = isMali
  || m_adapter->matchesDriver(VK_DRIVER_ID_MESA_TURNIP)
  || m_adapter->matchesDriver(VK_DRIVER_ID_QUALCOMM_PROPRIETARY)
  || ...; // existing list
applyTristate(tilerMode, m_options.tilerMode);
hints.preferRenderPassOps = tilerMode;
hints.preferCachedMemory = tilerMode;

// Mali-specific: prefer dynamic rendering
if (isMali && m_options.enableDynamicRendering)
  hints.preferDynamicRendering = true;

// Mali-specific: descriptor byte offsets (like RADV)
if (isMali)
  hints.preferDescriptorByteOffsets = true;

// Mali-specific: compute mip gen on Valhall
if (isMali && maliArch == MaliArchitecture::Valhall)
  hints.preferComputeMipGen = true;
```

- [ ] **Step 6: Run test to verify it passes**

Run: `meson test -C build test_mali_detection`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add src/dxvk/dxvk_adapter.cpp src/dxvk/dxvk_adapter.h
git add src/dxvk/dxvk_device.cpp src/dxvk/dxvk_options.cpp src/dxvk/dxvk_options.h
git add tests/dxvk/test_mali_detection.cpp
git commit -m "feat: Mali GPU detection and configuration options"
```

---

## Task 2: Dynamic Rendering & Render Pass Optimization

**Files:**
- Modify: `src/dxvk/dxvk_renderpass.cpp/h` (dynamic rendering support)
- Modify: `src/dxvk/dxvk_graphics.cpp/h` (pipeline state for dynamic rendering)
- Modify: `src/dxvk/dxvk_cmdlist.cpp/h` (command recording with dynamic rendering)
- Modify: `src/dxvk/dxvk_framebuffer.cpp/h` (framebuffer-less rendering)

**Interfaces:**
- Consumes: `DxvkDeviceHints::preferDynamicRendering`, `VK_KHR_dynamic_rendering` features
- Produces: `DxvkRenderPass` dynamic rendering path, `DxvkGraphicsPipeline` without render pass

- [ ] **Step 1: Write failing test for dynamic rendering**

```cpp
// tests/dxvk/test_dynamic_rendering.cpp
TEST(DynamicRendering, CreatesPipelineWithoutRenderPass) {
  // Create graphics pipeline with dynamic rendering info
  // Verify VkGraphicsPipelineCreateInfo has pDynamicRendering
}
TEST(DynamicRendering, RecordsCmdBeginRendering) {
  // Record draw command with dynamic rendering
  // Verify vkCmdBeginRenderingKHR called with correct attachments
}
TEST(DynamicRendering, FallsBackToRenderPassWhenDisabled) {
  // Disable dynamic rendering via config
  // Verify traditional render pass path used
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `meson test -C build test_dynamic_rendering`
Expected: FAIL

- [ ] **Step 3: Implement dynamic rendering in `src/dxvk/dxvk_renderpass.cpp/h`**

```cpp
// dxvk_renderpass.h - add to DxvkRenderPass:
struct DynamicRenderingInfo {
  VkRenderingFlagsKHR flags = 0;
  VkRect2D renderArea = {};
  uint32_t layerCount = 1;
  uint32_t viewMask = 0;
  std::vector<VkRenderingAttachmentInfoKHR> colorAttachments;
  std::optional<VkRenderingAttachmentInfoKHR> depthAttachment;
  std::optional<VkRenderingAttachmentInfoKHR> stencilAttachment;
};

class DxvkRenderPass {
public:
  // ... existing ...
  bool useDynamicRendering() const { return m_dynamicRenderingInfo.has_value(); }
  const DynamicRenderingInfo* getDynamicRenderingInfo() const { return m_dynamicRenderingInfo ? &*m_dynamicRenderingInfo : nullptr; }
  void setDynamicRenderingInfo(DynamicRenderingInfo&& info) { m_dynamicRenderingInfo = std::move(info); }
private:
  std::optional<DynamicRenderingInfo> m_dynamicRenderingInfo;
};
```

- [ ] **Step 4: Update pipeline creation in `src/dxvk/dxvk_graphics.cpp`**

```cpp
// In DxvkGraphicsPipeline::createPipeline():
VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
if (renderPass->useDynamicRendering() && m_device->features().khrDynamicRendering.dynamicRendering) {
  VkPipelineRenderingCreateInfoKHR dynRender = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR };
  dynRender.viewMask = renderPass->getDynamicRenderingInfo()->viewMask;
  dynRender.colorAttachmentCount = renderPass->getDynamicRenderingInfo()->colorAttachments.size();
  dynRender.pColorAttachmentFormats = colorFormats.data();
  dynRender.depthAttachmentFormat = depthFormat;
  dynRender.stencilAttachmentFormat = stencilFormat;
  createInfo.pNext = &dynRender;
  createInfo.renderPass = VK_NULL_HANDLE;
  createInfo.subpass = 0;
} else {
  createInfo.renderPass = renderPass->getRenderPass();
  createInfo.subpass = renderPass->getSubpassIndex();
}
```

- [ ] **Step 5: Update command recording in `src/dxvk/dxvk_cmdlist.cpp`**

```cpp
// In DxvkCommandList::beginRenderPass():
if (renderPass->useDynamicRendering() && m_device->features().khrDynamicRendering.dynamicRendering) {
  VkRenderingInfoKHR renderInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO_KHR };
  renderInfo.renderArea = renderPass->getDynamicRenderingInfo()->renderArea;
  renderInfo.layerCount = renderPass->getDynamicRenderingInfo()->layerCount;
  renderInfo.viewMask = renderPass->getDynamicRenderingInfo()->viewMask;
  renderInfo.colorAttachmentCount = renderPass->getDynamicRenderingInfo()->colorAttachments.size();
  renderInfo.pColorAttachments = renderPass->getDynamicRenderingInfo()->colorAttachments.data();
  renderInfo.pDepthAttachment = renderPass->getDynamicRenderingInfo()->depthAttachment ? &*renderPass->getDynamicRenderingInfo()->depthAttachment : nullptr;
  renderInfo.pStencilAttachment = renderPass->getDynamicRenderingInfo()->stencilAttachment ? &*renderPass->getDynamicRenderingInfo()->stencilAttachment : nullptr;
  m_vkd->vkCmdBeginRenderingKHR(m_commandBuffer, &renderInfo);
} else {
  // Existing traditional render pass begin
}
```

- [ ] **Step 6: Run test to verify it passes**

Run: `meson test -C build test_dynamic_rendering`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add src/dxvk/dxvk_renderpass.cpp src/dxvk/dxvk_renderpass.h
git add src/dxvk/dxvk_graphics.cpp src/dxvk/dxvk_cmdlist.cpp
git add tests/dxvk/test_dynamic_rendering.cpp
git commit -m "feat: Dynamic rendering support for Mali (VK_KHR_dynamic_rendering)"
```

---

## Task 3: Shader Compilation Optimizations (FP16/INT8, Subgroup, GPL)

**Files:**
- Modify: `src/dxvk/dxvk_shader_spirv.cpp/h` (SPIR-V generation with FP16/INT8)
- Modify: `src/dxvk/dxvk_shader_ir.cpp/h` (IR optimization for Valhall)
- Modify: `src/dxvk/dxvk_pipemanager.cpp/h` (Graphics Pipeline Library)
- Modify: `src/dxvk/dxvk_shader_cache.cpp/h` (Persistent cache optimization)

**Interfaces:**
- Consumes: `DxvkOptions` shader flags, `VkPhysicalDeviceVulkan13Features` (shaderFloat16, shaderInt8, subgroupSizeControl), `VK_EXT_graphics_pipeline_library` features
- Produces: Optimized SPIR-V modules, pre-compiled pipeline libraries

- [ ] **Step 1: Write failing tests**

```cpp
// tests/dxvk/test_shader_optimization.cpp
TEST(ShaderFP16, GeneratesFP16OpsWhenEnabled) {
  // Compile shader with FP16 enable
  // Verify SPIR-V contains OpTypeFloat 16 and FP16 arithmetic
}
TEST(ShaderINT8, GeneratesINT8OpsWhenEnabled) {
  // Compile shader with INT8 enable
  // Verify SPIR-V contains OpTypeInt 8 and INT8 arithmetic
}
TEST(ShaderSubgroup, EmitsSubgroupOpsForValhall) {
  // Compile compute shader with subgroup enable
  // Verify OpGroupNonUniformQuadBroadcast, OpSubgroupBallotKHR, etc.
}
TEST(GPL, CreatesPipelineLibrary) {
  // Enable GPL, create graphics pipeline
  // Verify VkPipelineLibraryCreateInfoKHR used
}
TEST(ShaderCache, CompressesCacheForMali) {
  // Save/load shader cache
  // Verify cache size reduced vs baseline
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `meson test -C build test_shader_optimization`
Expected: FAIL

- [ ] **Step 3: Implement FP16/INT8 in `src/dxvk/dxvk_shader_spirv.cpp`**

```cpp
// In DxvkSpirvShader::compile() or SPIR-V generation:
if (m_options.enableFP16Shaders && m_device->features().vk13.shaderFloat16) {
  // Add OpCapability Float16
  // Add OpExtension "SPV_KHR_16bit_storage" if needed
  // In shader IR: lower FP32 math to FP16 where precision allows
}
if (m_options.enableINT8Shaders && m_device->features().vk13.shaderInt8) {
  // Add OpCapability Int8
  // In shader IR: lower int32 to int8 for indices, uniforms
}
// Subgroup ops for Valhall
if (m_device->features().vk13.subgroupSizeControl) {
  // Set SubgroupSize decoration for wave64 (Mali Valhall native)
  // Emit OpSubgroupBallotKHR, OpSubgroupShuffle, OpGroupNonUniformQuad*
}
```

- [ ] **Step 4: Implement IR optimizations in `src/dxvk/dxvk_shader_ir.cpp`**

```cpp
// In DxvkShaderIR::optimize():
if (m_options.enableFP16Shaders) {
  // Pass: Demote FP32 to FP16 for:
  //   - Texture coordinates (sampling)
  //   - Color calculations (blending, lighting)
  //   - Position/UV math
  // Keep FP32 for: Position output, depth, precise math
}
if (m_device->isMali() && m_device->getMaliArchitecture() == MaliArchitecture::Valhall) {
  // Valhall-specific:
  //   - Use BFM/BFI for bitfield ops
  //   - Use FMA for multiply-add
  //   - Optimize transcendental (sin/cos) with polynomial
  //   - Use subgroup quad ops for derivative approximation
}
```

- [ ] **Step 5: Implement GPL in `src/dxvk/dxvk_pipemanager.cpp`**

```cpp
// In DxvkPipelineManager::createGraphicsPipeline():
if (m_options.enableGraphicsPipelineLibrary && m_device->features().extGraphicsPipelineLibrary.graphicsPipelineLibrary) {
  // Create pipeline libraries for:
  //   - Vertex input (VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT)
  //   - Pre-rasterization (VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
  //   - Fragment shader (VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)
  //   - Fragment output (VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT)
  // Link at draw time with vkCmdBindPipelineShaderGroupNV or vkCreateGraphicsPipelines with library linking
}
```

- [ ] **Step 6: Optimize shader cache in `src/dxvk/dxvk_shader_cache.cpp`**

```cpp
// In DxvkShaderCache::storePipeline():
if (m_device->isMali()) {
  // Compress SPIR-V with spirv-remap or custom Mali-specific remapping
  // Store pipeline library blobs separately for GPL
  // Use ZSTD compression for cache files
}
// In DxvkShaderCache::loadPipeline():
//   Decompress, remap descriptors for current device
```

- [ ] **Step 7: Run test to verify it passes**

Run: `meson test -C build test_shader_optimization`
Expected: PASS

- [ ] **Step 8: Commit**

```bash
git add src/dxvk/dxvk_shader_spirv.cpp src/dxvk/dxvk_shader_ir.cpp
git add src/dxvk/dxvk_pipemanager.cpp src/dxvk/dxvk_shader_cache.cpp
git add tests/dxvk/test_shader_optimization.cpp
git commit -m "feat: Shader optimizations for Mali (FP16/INT8, Subgroup, GPL, Cache)"
```

---

## Task 4: Descriptor Indexing, Buffer Device Address & Unified Memory

**Files:**
- Modify: `src/dxvk/dxvk_descriptor_pool.cpp/h` (Descriptor indexing / bindless)
- Modify: `src/dxvk/dxvk_buffer.cpp/h` (Buffer Device Address)
- Modify: `src/dxvk/dxvk_memory.cpp/h` (Unified memory allocator)
- Modify: `src/dxvk/dxvk_format.cpp/h` (Format support for Mali)

**Interfaces:**
- Consumes: `VK_EXT_descriptor_indexing`, `VK_KHR_buffer_device_address`, `VK_EXT_memory_budget`, `VK_EXT_memory_priority`
- Produces: Bindless descriptor sets, GPU virtual addresses, Memory budget tracking

- [ ] **Step 1: Write failing tests**

```cpp
// tests/dxvk/test_descriptor_memory.cpp
TEST(DescriptorIndexing, CreatesBindlessDescriptorSet) {
  // Create descriptor set with descriptorIndexing
  // Verify VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT set
}
TEST(BufferDeviceAddress, ReturnsGPUAddress) {
  // Create buffer with BDA usage
  // Verify vkGetBufferDeviceAddressKHR returns valid address
}
TEST(UnifiedMemory, RespectsBudgetLimit) {
  // Set maxMemoryBudgetMB = 1024
  // Allocate > 1024MB, verify defrag/fail
}
TEST(FormatSupport, MaliFormatsSupported) {
  // Query format features for ASTC, ETC2, RGBA10X6
  // Verify optimal tiling features present
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `meson test -C build test_descriptor_memory`
Expected: FAIL

- [ ] **Step 3: Implement bindless descriptors in `src/dxvk/dxvk_descriptor_pool.cpp`**

```cpp
// In DxvkDescriptorPool::createDescriptorSet():
if (m_device->options().enableDescriptorIndexing && m_device->features().vk12.descriptorIndexing) {
  // Use VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
  // Use VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
  // Use VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT for dynamic updates
  // Allocate large descriptor arrays (up to maxPerStageDescriptorSampledImages)
}
```

- [ ] **Step 4: Implement BDA in `src/dxvk/dxvk_buffer.cpp`**

```cpp
// In DxvkBuffer::create():
VkBufferCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
if (m_device->options().enableBufferDeviceAddress && m_device->features().vk13.bufferDeviceAddress) {
  info.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  info.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
}

// In DxvkBuffer::getDeviceAddress():
if (m_deviceAddress == 0 && m_device->features().vk13.bufferDeviceAddress) {
  VkBufferDeviceAddressInfoKHR addrInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR };
  addrInfo.buffer = m_buffer;
  m_deviceAddress = m_vkd->vkGetBufferDeviceAddressKHR(m_vkd->device(), &addrInfo);
}
return m_deviceAddress;
```

- [ ] **Step 5: Implement unified memory in `src/dxvk/dxvk_memory.cpp`**

```cpp
// In DxvkMemoryAllocator::alloc():
if (m_device->isMali()) {
  // Mali unified memory: prefer VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  // for frequently mapped buffers (UBO, staging)
  // Use VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT for textures, but it's same heap
  // Apply memory budget:
  if (m_options.maxMemoryBudget > 0) {
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = queryMemoryBudget();
    if (budget.heapUsage[0] + size > m_options.maxMemoryBudget) {
      defragment();
      if (still over budget) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
  }
}

// Add memory priority for Mali:
if (m_device->features().extMemoryPriority.memoryPriority) {
  VkMemoryPriorityAllocateInfoEXT priorityInfo = { VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT };
  priorityInfo.priority = isCriticalResource ? 1.0f : 0.5f;
  allocInfo.pNext = &priorityInfo;
}
```

- [ ] **Step 6: Run test to verify it passes**

Run: `meson test -C build test_descriptor_memory`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add src/dxvk/dxvk_descriptor_pool.cpp src/dxvk/dxvk_buffer.cpp
git add src/dxvk/dxvk_memory.cpp src/dxvk/dxvk_format.cpp
git add tests/dxvk/test_descriptor_memory.cpp
git commit -m "feat: Descriptor indexing, BDA, and unified memory for Mali"
```

---

## Task 5: ARM-Specific Extensions (Builtins, Tile Image, Raster Order)

**Files:**
- Modify: `src/dxvk/dxvk_shader_spirv.cpp` (ARM builtins injection)
- Modify: `src/dxvk/dxvk_graphics.cpp` (Shader tile image, fragment shading rate)
- Modify: `src/dxvk/dxvk_cmdlist.cpp` (Rasterization order attachment access)
- Create: `src/dxvk/dxvk_mali_extensions.cpp/h` (ARM extension wrappers)

**Interfaces:**
- Consumes: `VK_ARM_shader_core_builtins`, `VK_EXT_shader_tile_image`, `VK_ARM_rasterization_order_attachment_access`, `VK_KHR_fragment_shading_rate`
- Produces: Shader core builtins intrinsics, tile memory load/store, raster-order views, VRS

- [ ] **Step 1: Write failing tests**

```cpp
// tests/dxvk/test_arm_extensions.cpp
TEST(ARMBuiltins, InjectsCoreBuiltinsInShader) {
  // Compile shader with ARM builtins enabled
  // Verify SPIR-V contains OpExtension "SPV_ARM_shader_core_builtins"
  // Verify OpBuiltinARM core ID, lane ID, quad ops
}
TEST(ShaderTileImage, EmitsTileLoadStore) {
  // Fragment shader with tile image access
  // Verify OpTileImageReadARM, OpTileImageWriteARM
}
TEST(RasterOrder, CreatesRasterOrderView) {
  // Enable rasterization order attachment access
  // Verify VkRasterizationOrderAttachmentAccessCreateInfoARM in pipeline
}
TEST(FragmentShadingRate, SetsShadingRate) {
  // Set VRS via pipeline or primitive
  // Verify VkPipelineFragmentShadingRateStateCreateInfoKHR
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `meson test -C build test_arm_extensions`
Expected: FAIL

- [ ] **Step 3: Implement ARM builtins in `src/dxvk/dxvk_shader_spirv.cpp`**

```cpp
// In SPIR-V generation for fragment/compute shaders:
if (m_options.enableARMBuiltins && m_device->features().armShaderCoreBuiltins.shaderCoreBuiltins) {
  // Add OpExtension "SPV_ARM_shader_core_builtins"
  // Add OpCapability ShaderCoreBuiltinsARM
  // Inject builtins for:
  //   - Core ID: OpBuiltinARM CoreIdARM
  //   - Lane ID: OpBuiltinARM LaneIdARM
  //   - Quad ops: OpGroupNonUniformQuadBroadcast, OpGroupNonUniformQuadSwap
  //   - Tile coords: OpBuiltinARM TileCoordARM (if tile image enabled)
}
```

- [ ] **Step 4: Implement shader tile image in `src/dxvk/dxvk_graphics.cpp`**

```cpp
// In fragment shader compilation for Mali Valhall:
if (m_options.enableShaderTileImage && m_device->features().extShaderTileImage.shaderTileImageColorReadAccess) {
  // For render passes using dynamic rendering with tile image:
  // Add tile image declarations to SPIR-V
  // Use OpTileImageReadARM for input attachment reads (avoid resolve)
  // Use OpTileImageWriteARM for output writes (direct tile memory)
  // Requires VK_EXT_shader_tile_image + dynamic rendering
}
```

- [ ] **Step 5: Implement raster order in `src/dxvk/dxvk_cmdlist.cpp`**

```cpp
// In pipeline creation for transparency:
if (m_device->features().armRasterizationOrderAttachmentAccess.rasterizationOrderColorAttachmentAccess) {
  VkRasterizationOrderAttachmentAccessCreateInfoARM rasterOrder = { VK_STRUCTURE_TYPE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_CREATE_INFO_ARM };
  rasterOrder.colorAttachmentAccess = VK_TRUE;
  rasterOrder.depthAttachmentAccess = VK_TRUE;
  rasterOrder.stencilAttachmentAccess = VK_TRUE;
  pipelineInfo.pNext = &rasterOrder;
}
```

- [ ] **Step 6: Implement VRS in `src/dxvk/dxvk_graphics.cpp`**

```cpp
// In DxvkGraphicsPipeline::createPipeline():
if (m_device->features().khrFragmentShadingRate.pipelineFragmentShadingRate) {
  VkPipelineFragmentShadingRateStateCreateInfoKHR fsrState = { VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR };
  fsrState.fragmentSize = { 2, 2 }; // 2x2 VRS for performance
  fsrState.primitiveFragmentShadingRate = VK_PRIMITIVE_FRAGMENT_SHADING_RATE_2X2_KHR;
  // Chain into pipeline create info
}
```

- [ ] **Step 7: Run test to verify it passes**

Run: `meson test -C build test_arm_extensions`
Expected: PASS

- [ ] **Step 8: Commit**

```bash
git add src/dxvk/dxvk_shader_spirv.cpp src/dxvk/dxvk_graphics.cpp
git add src/dxvk/dxvk_cmdlist.cpp src/dxvk/dxvk_mali_extensions.cpp src/dxvk/dxvk_mali_extensions.h
git add tests/dxvk/test_arm_extensions.cpp
git commit -m "feat: ARM-specific extensions (Builtins, Tile Image, Raster Order, VRS)"
```

---

## Task 6: Synchronization2 & Timeline Semaphores

**Files:**
- Modify: `src/dxvk/dxvk_barrier.cpp/h` (Synchronization2 barriers)
- Modify: `src/dxvk/dxvk_fence.cpp/h` (Timeline semaphore support)
- Modify: `src/dxvk/dxvk_queue.cpp/h` (Queue submit with timeline)
- Modify: `src/dxvk/dxvk_cmdlist.cpp` (Cmd buffer barriers)

**Interfaces:**
- Consumes: `VK_KHR_synchronization2`, `VK_KHR_timeline_semaphore`
- Produces: Fine-grained pipeline barriers, Timeline semaphore submit

- [ ] **Step 1: Write failing tests**

```cpp
// tests/dxvk/test_synchronization.cpp
TEST(Sync2, UsesVkCmdPipelineBarrier2) {
  // Record barrier with synchronization2
  // Verify vkCmdPipelineBarrier2KHR called with VkDependencyInfoKHR
}
TEST(TimelineSemaphore, SignalWaitTimeline) {
  // Submit with timeline semaphore signal/wait
  // Verify VkTimelineSemaphoreSubmitInfoKHR in submit
}
TEST(Sync2, FineGrainedBarriers) {
  // Image layout transition with sync2
  // Verify srcStageMask/dstStageMask use VK_PIPELINE_STAGE_2_* flags
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `meson test -C build test_synchronization`
Expected: FAIL

- [ ] **Step 3: Implement Synchronization2 in `src/dxvk/dxvk_barrier.cpp`**

```cpp
// In DxvkBarrier::record():
if (m_device->features().khrSynchronization2.synchronization2 && m_options.enableSynchronization2) {
  VkDependencyInfoKHR depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR };
  depInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
  depInfo.memoryBarrierCount = memoryBarriers.size();
  depInfo.pMemoryBarriers = memoryBarriers.data();
  depInfo.bufferMemoryBarrierCount = bufferBarriers.size();
  depInfo.pBufferMemoryBarriers = bufferBarriers.data();
  depInfo.imageMemoryBarrierCount = imageBarriers.size();
  depInfo.pImageMemoryBarriers = imageBarriers.data();
  m_vkd->vkCmdPipelineBarrier2KHR(m_commandBuffer, &depInfo);
} else {
  // Fallback to vkCmdPipelineBarrier
}
```

- [ ] **Step 4: Implement timeline semaphores in `src/dxvk/dxvk_fence.cpp` and `dxvk_queue.cpp`**

```cpp
// In DxvkQueue::submit():
if (m_device->features().khrTimelineSemaphore.timelineSemaphore && m_options.enableTimelineSemaphore) {
  VkTimelineSemaphoreSubmitInfoKHR timelineInfo = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR };
  timelineInfo.waitSemaphoreValueCount = waitValues.size();
  timelineInfo.pWaitSemaphoreValues = waitValues.data();
  timelineInfo.signalSemaphoreValueCount = signalValues.size();
  timelineInfo.pSignalSemaphoreValues = signalValues.data();
  submitInfo.pNext = &timelineInfo;
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `meson test -C build test_synchronization`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/dxvk/dxvk_barrier.cpp src/dxvk/dxvk_fence.cpp
git add src/dxvk/dxvk_queue.cpp src/dxvk/dxvk_cmdlist.cpp
git add tests/dxvk/test_synchronization.cpp
git commit -m "feat: Synchronization2 and Timeline Semaphores for Mali"
```

---

## Task 7: Android/Termux Build & WSI Integration

**Files:**
- Modify: `meson.build` (Android cross-compile options)
- Modify: `build-win64.txt`, `build-win32.txt` (MinGW cross files)
- Create: `build-android-aarch64.txt` (Android NDK cross file)
- Modify: `src/vulkan/vulkan_loader.cpp` (Android loader)
- Modify: `package-release.sh` (Android packaging)
- Create: `scripts/build-android.sh` (Android build script)

**Interfaces:**
- Consumes: Android NDK r26+, SDL3 for WSI, MinGW-w64 11+
- Produces: Windows DLLs (x86_64/i686), Linux .so (aarch64), Android .so (aarch64)

- [ ] **Step 1: Write failing tests**

```bash
# tests/build/test_android_build.sh
#!/bin/bash
set -e
./scripts/build-android.sh
# Verify outputs exist:
test -f build-android/aarch64/libdxvk_d3d11.so
test -f build-android/aarch64/libdxvk_dxgi.so
# Verify symbols:
nm build-android/aarch64/libdxvk_d3d11.so | grep -E "Direct3DCreate|DXGI"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tests/build/test_android_build.sh`
Expected: FAIL (script doesn't exist)

- [ ] **Step 3: Create Android cross file `build-android-aarch64.txt`**

```ini
[binaries]
c = '/opt/ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
cpp = '/opt/ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang++'
ar = '/opt/ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar'
strip = '/opt/ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip'
pkgconfig = 'pkg-config'

[host_machine]
system = 'linux'
cpu_family = 'aarch64'
cpu = 'cortex-a78'
endian = 'little'

[properties]
sys_root = '/opt/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot'
```

- [ ] **Step 4: Create build script `scripts/build-android.sh`**

```bash
#!/bin/bash
set -e
NDK_PATH=${ANDROID_NDK_HOME:-/opt/ndk}
API_LEVEL=29
ABI=arm64-v8a

meson setup --cross-file build-android-aarch64.txt \
  --buildtype release \
  -Dnative_sdl3=enabled \
  -Dnative_sdl2=disabled \
  -Dnative_glfw=disabled \
  --prefix /tmp/dxvm-android \
  build-android

meson compile -C build-android
meson install -C build-android --skip-subprojects

# Package for Winlator/Box64
mkdir -p dist/android/aarch64
cp /tmp/dxvm-android/lib/*.so dist/android/aarch64/
```

- [ ] **Step 5: Update WSI for Android in `src/vulkan/vulkan_loader.cpp`**

```cpp
// Add Android surface creation:
#ifdef VK_USE_PLATFORM_ANDROID_KHR
VkResult DxvkInstance::createAndroidSurface(ANativeWindow* window, VkSurfaceKHR* surface) {
  VkAndroidSurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR };
  createInfo.window = window;
  return vki()->vkCreateAndroidSurfaceKHR(vki()->instance(), &createInfo, nullptr, surface);
}
#endif
```

- [ ] **Step 6: Run test to verify it passes**

Run: `bash tests/build/test_android_build.sh`
Expected: PASS (produces .so files with correct exports)

- [ ] **Step 7: Commit**

```bash
git add build-android-aarch64.txt scripts/build-android.sh
git add src/vulkan/vulkan_loader.cpp package-release.sh meson.build
git add tests/build/test_android_build.sh
git commit -m "feat: Android/Termux build system and WSI integration"
```

---

## Task 8: D3D9/D3D11 Vendor Reporting & Game Compatibility

**Files:**
- Modify: `src/d3d9/d3d9_adapter.cpp` (Vendor ID reporting for Mali)
- Modify: `src/d3d11/d3d11_device.cpp` (Vendor hacks for Mali)
- Modify: `src/dxgi/dxgi_adapter.cpp` (DXGI adapter description)

**Interfaces:**
- Consumes: `DxvkAdapter::isMali()`, `DxvkAdapter::getMaliArchitecture()`
- Produces: Correct vendor ID (0x13B5 for ARM), device description, driver version

- [ ] **Step 1: Write failing tests**

```cpp
// tests/dxvk/test_vendor_reporting.cpp
TEST(VendorReporting, D3D9ReportsARMVendor) {
  // Create D3D9 adapter on Mali
  // Verify GetAdapterIdentifier returns VendorId = 0x13B5
}
TEST(VendorReporting, DXGIReportsMaliDescription) {
  // Enum DXGI adapters
  // Verify description contains "Mali-G615"
}
TEST(VendorHacks, MaliSpecificWorkaroundsApplied) {
  // Create D3D11 device on Mali
  // Verify vendor hacks for Mali applied (e.g., no NVAPI)
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `meson test -C build test_vendor_reporting`
Expected: FAIL

- [ ] **Step 3: Update D3D9 adapter in `src/d3d9/d3d9_adapter.cpp`**

```cpp
// In D3D9Adapter::InitAdapter():
if (m_dxvkAdapter->isMali()) {
  m_vendorId = 0x13B5; // ARM PCI vendor ID
  m_deviceId = m_dxvkAdapter->info().deviceId; // Mali device ID
  // Set driver description
  snprintf(m_driverDescription, sizeof(m_driverDescription),
           "DXVM Mali %s (Vulkan %d.%d.%d)",
           m_dxvkAdapter->getMaliArchitecture() == MaliArchitecture::Valhall ? "Valhall" : "Bifrost",
           VK_VERSION_MAJOR(props.core.properties.apiVersion),
           VK_VERSION_MINOR(props.core.properties.apiVersion),
           VK_VERSION_PATCH(props.core.properties.apiVersion));
}
```

- [ ] **Step 4: Update DXGI adapter in `src/dxgi/dxgi_adapter.cpp`**

```cpp
// In DxgiAdapter::initAdapterDesc():
if (m_dxvkAdapter->isMali()) {
  desc.VendorId = 0x13B5;
  desc.DeviceId = m_dxvkAdapter->info().deviceId;
  wcscpy(desc.Description, L"Mali-G615 MC2 (DXVM)");
  // Report as UMA (Unified Memory Architecture)
  desc.DedicatedVideoMemory = 0;
  desc.DedicatedSystemMemory = 0;
  desc.SharedSystemMemory = m_dxvkAdapter->info().deviceMemory;
}
```

- [ ] **Step 5: Update D3D11 vendor hacks in `src/d3d11/d3d11_device.cpp`**

```cpp
// In D3D11Device::initVendorHacks():
if (m_dxvkAdapter->isMali()) {
  // Mali doesn't need NVAPI/AMDAGS workarounds
  m_vendorHacks.hideNvidiaGpu = true;
  m_vendorHacks.hideAmdGpu = true;
  // Mali-specific: disable features not supported
  m_vendorHacks.disableDualSrcBlend = true; // dualSrcBlend = false on Mali
  m_vendorHacks.disableLogicOp = true;      // logicOp = false on Mali
}
```

- [ ] **Step 6: Run test to verify it passes**

Run: `meson test -C build test_vendor_reporting`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add src/d3d9/d3d9_adapter.cpp src/dxgi/dxgi_adapter.cpp
git add src/d3d11/d3d11_device.cpp
git add tests/dxvk/test_vendor_reporting.cpp
git commit -m "feat: Mali vendor reporting and D3D9/11/DXGI compatibility"
```

---

## Task 9: Mali-G615 Default Config Preset

**Files:**
- Modify: `dxvk.conf` (Add Mali-G615 preset section)
- Create: `dxvk-mali-g615.conf` (Standalone preset file)

**Interfaces:**
- Consumes: All Mali config options from Task 1
- Produces: Ready-to-use config for Mali-G615 users

- [ ] **Step 1: Write failing test**

```bash
# tests/config/test_mali_preset.sh
#!/bin/bash
# Verify config file has all Mali options set correctly
grep -q "enableDynamicRendering = True" dxvk-mali-g615.conf
grep -q "enableFP16Shaders = True" dxvk-mali-g615.conf
grep -q "enableINT8Shaders = True" dxvk-mali-g615.conf
grep -q "enableDescriptorIndexing = True" dxvk-mali-g615.conf
grep -q "enableBufferDeviceAddress = True" dxvk-mali-g615.conf
grep -q "enableShaderTileImage = True" dxvk-mali-g615.conf
grep -q "enableARMBuiltins = True" dxvk-mali-g615.conf
grep -q "enableSynchronization2 = True" dxvk-mali-g615.conf
grep -q "tilerMode = True" dxvk-mali-g615.conf
grep -q "maxMemoryBudget = 3072" dxvk-mali-g615.conf
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash tests/config/test_mali_preset.sh`
Expected: FAIL

- [ ] **Step 3: Create `dxvk-mali-g615.conf`**

```ini
# DXVM Mali-G615 MC2 Optimal Preset
# Based on Vulkan 1.3.247, driver v1.r44p1+
# For Valhall architecture (G610, G615, G710, G715, G720)

[dxvk]
# Rendering - Dynamic rendering eliminates render pass overhead
enableDynamicRendering = True
enableGraphicsPipelineLibrary = True

# Shaders - Valhall native FP16/INT8, subgroup ops
enableFP16Shaders = True
enableINT8Shaders = True
enableDescriptorIndexing = True
enableBufferDeviceAddress = True
enableShaderTileImage = True
enableARMBuiltins = True
lowerSinCos = Auto

# Memory - Unified memory optimization
enableMemoryDefrag = True
maxMemoryBudget = 3072
preferCachedMemory = True
tilerMode = True

# Synchronization - Fine-grained, low overhead
enableSynchronization2 = True
enableTimelineSemaphore = True

# Pipeline
pipelineCreationCacheControl = True
enableDescriptorHeap = Auto
enableDescriptorBuffer = False

# HUD for profiling
hud = devinfo,fps,frametimes,gpuload,compiler,drawcalls,memory
```

- [ ] **Step 4: Run test to verify it passes**

Run: `bash tests/config/test_mali_preset.sh`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add dxvk-mali-g615.conf dxvk.conf
git add tests/config/test_mali_preset.sh
git commit -m "feat: Mali-G615 default configuration preset"
```

---

## Task 10: CI/CD & Regression Testing

**Files:**
- Create: `.github/workflows/mali-ci.yml` (GitHub Actions CI)
- Create: `scripts/test-games.sh` (Game compatibility test script)
- Modify: `meson.build` (Add test dependencies)

**Interfaces:**
- Consumes: All previous tasks
- Produces: Automated testing pipeline

- [ ] **Step 1: Create CI workflow `.github/workflows/mali-ci.yml`**

```yaml
name: Mali CI
on: [push, pull_request]
jobs:
  build-linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install deps
        run: sudo apt-get update && sudo apt-get install -y meson ninja-build mingw-w64 glslang-tools
      - name: Build Linux x86_64
        run: meson setup --cross-file build-win64.txt build.w64 && meson compile -C build.w64
      - name: Run unit tests
        run: meson test -C build.w64

  build-android:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Setup Android NDK
        uses: android-actions/setup-android@v3
        with: { ndk-version: r26c }
      - name: Build Android aarch64
        run: ./scripts/build-android.sh
      - name: Verify outputs
        run: test -f dist/android/aarch64/libdxvk_d3d11.so

  test-games:
    needs: [build-linux, build-android]
    runs-on: ubuntu-latest
    if: github.event_name == 'schedule' || github.repository == 'danzfadlullah/DXVM'
    steps:
      - uses: actions/checkout@v4
      - name: Run game compatibility tests
        run: ./scripts/test-games.sh
```

- [ ] **Step 2: Create game test script `scripts/test-games.sh`**

```bash
#!/bin/bash
# Test suite for game compatibility (run in Wine/Box64 environment)
GAMES=(
  "Game1.exe"  # Lightweight D3D11 test
  "Game2.exe"  # D3D9 test
)
for game in "${GAMES[@]}"; do
  echo "Testing $game..."
  DXVK_HUD=fps,compiler WINEPREFIX=/tmp/wine-test wine "$game" -benchmark -quit
  # Check exit code, FPS, no crashes
done
```

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/mali-ci.yml scripts/test-games.sh
git commit -m "ci: Add Mali CI/CD and game regression testing"
```

---

## Execution Order & Dependencies

```
Task 1 (Detection & Config)
    ↓
Task 2 (Dynamic Rendering) ← Task 6 (Sync2) can parallel
    ↓
Task 3 (Shader Optimizations)
    ↓
Task 4 (Descriptor & Memory)
    ↓
Task 5 (ARM Extensions)
    ↓
Task 7 (Android Build)
    ↓
Task 8 (Vendor Reporting)
    ↓
Task 9 (Config Preset)
    ↓
Task 10 (CI/CD)
```

---

## Summary

| Task | Component | Est. Effort | Priority |
|------|-----------|-------------|----------|
| 1 | Mali Detection & Config | 2 days | Critical |
| 2 | Dynamic Rendering | 3 days | Critical |
| 3 | Shader Optimizations | 5 days | Critical |
| 4 | Descriptor & Memory | 3 days | Critical |
| 5 | ARM Extensions | 4 days | High |
| 6 | Synchronization2 | 2 days | High |
| 7 | Android Build | 3 days | Critical |
| 8 | Vendor Reporting | 1 day | Medium |
| 9 | Config Preset | 0.5 day | Medium |
| 10 | CI/CD | 1 day | Low |

**Total**: ~24.5 days (5 weeks) for full implementation

---

## Next Steps

After plan review and approval, execute with:
- **Subagent-driven**: `superpowers:subagent-driven-development` (recommended for independent review)
- **Native**: `superpowers:executing-plans` (faster, single context)

**Recommendation**: Subagent-driven because tasks have clear interfaces but deep Vulkan knowledge required; independent review catches driver-specific bugs early.