# DXVM Mali-G615 Optimization Design Document

**Date**: 2026-11-19  
**Project**: DXVM (DirectX to Vulkan for Mali)  
**Base**: DXVK fork from danzfadlullah/DXVM  
**Target GPU**: Mali-G615 MC2 (ARM Valhall architecture)  
**Target Platform**: Android (Termux/proot) via Winlator/Box64  
**Vulkan API Version**: 1.3.247

---

## Intent and Why

Membuat versi DXVK yang dioptimalkan khusus untuk GPU Mali-G615 MC2 (ARM Valhall) agar game Windows berjalan optimal di Android melalui Winlator/Box64. GPU ini mendukung Vulkan 1.3 penuh dengan banyak ekstensi modern, namun DXVK upstream belum memanfaatkan sepenuhnya fitur-fitur ARM-specific seperti shader core builtins, rasterization order attachment access, dan optimasi tiler.

---

## Goals

1. **Performance**: Meningkatkan frame rate 20-40% dibanding DXVK upstream pada Mali-G615
2. **Compatibility**: Dukungan penuh untuk D3D8/9/10/11 via Vulkan di Android
3. **Stability**: Mengurangi crash dan GPU hang pada driver Mali
4. **Shader Compilation**: Mengurangi stutter dengan Graphics Pipeline Library + shader cache yang efisien
5. **Memory Efficiency**: Optimasi unified memory (tidak ada VRAM dedicated)

---

## Non-Goals

- Dukungan untuk GPU non-ARM (NVIDIA, AMD, Intel) - meski tetap kompatibel
- Fitur Windows-specific (FSE, HDR, NVAPI) yang tidak relevan di Android
- Vulkan < 1.3 support (Mali-G615 sudah 1.3)

---

## Anti-Goals

- Jangan tambahkan overhead CPU berlebihan untuk tracking state
- Jangan gunakan fitur yang tidak stabil di driver Mali (seperti VK_EXT_descriptor_buffer sebelum stable)
- Jangan break kompatibilitas dengan Box64/Wine

---

## Constraints

| Constraint | Detail |
|------------|--------|
| **Platform** | Android 10+ (API 29+) via Termux/proot |
| **Architecture** | ARM64 (aarch64) host, x86_64 guest via Box64 |
| **Vulkan** | 1.3 minimum, driver Mali v1.r44p1+ |
| **Memory** | Unified memory (system RAM = VRAM), budget ~4-8GB |
| **CPU** | 4-8 core ARM Cortex-A78/A510, single-thread perf kritis |
| **Build** | Cross-compile via MinGW-w64 untuk Windows DLL, atau native Linux .so |

---

## Mali-G615 Capabilities Analysis

Berdasarkan `vulkan-info.json` yang disediakan:

### ✅ Fully Supported (Core Vulkan 1.3)
- `dynamicRendering` - **Critical**: Eliminasi render pass overhead
- `descriptorIndexing` + `runtimeDescriptorArray` - **Critical**: Bindless descriptors
- `bufferDeviceAddress` - **Critical**: GPU pointer access, lower overhead
- `synchronization2` - **Critical**: Fine-grained sync, less CPU overhead
- `shaderFloat16` + `shaderInt8` - **Critical**: Mali-native FP16/INT8 throughput
- `shaderIntegerDotProduct` - **Critical**: Dot product acceleration
- `subgroupSizeControl` + `computeFullSubgroups` - **Critical**: Wave64/32 optimization
- `timelineSemaphore` - **Critical**: Efficient CPU-GPU sync
- `imagelessFramebuffer` - **Critical**: Dynamic rendering support

### ✅ ARM-Specific Extensions
- `VK_ARM_shader_core_builtins` - **Unique**: Shader core ID, lane ID, quad ops
- `VK_ARM_rasterization_order_attachment_access` - **Unique**: Raster-order views
- `VK_ARM_shader_core_properties` - **Unique**: Core properties query

### ✅ Tiler-Friendly Features
- `VK_KHR_fragment_shading_rate` - Variable rate shading
- `VK_EXT_fragment_density_map` - Foveated rendering
- `VK_EXT_shader_tile_image` - Tile memory access (Valhall)
- `VK_KHR_multiview` - Stereo/VR rendering

### ⚠️ Limitations
- `dualSrcBlend: false` - No dual source blending
- `logicOp: false` - No logic operations
- `fillModeNonSolid: false` - No wireframe
- `depthBounds: false` - No depth bounds test
- `maxViewports: 1` - Single viewport only
- `maxFragmentDualSrcAttachments: 0` - No dual source blend
- `VK_EXT_depth_bias_control` - May not be supported

### Memory Properties (from vulkan-info)
- **Unified Memory**: Single heap (device local = system memory)
- **Max Allocation**: 16384 concurrent allocations
- **Max Uniform Buffer**: 2GB range
- **Max Storage Buffer**: 2GB range
- **Max Push Constants**: 256 bytes (small!)

---

## Approaches Considered

### Approach 1: Minimal Fork (Configuration Only)
**Description**: Hanya menambahkan preset konfigurasi `dxvk.conf` untuk Mali, tanpa mengubah kode.

**Trade-offs**:
- ✅ Mudah di-maintain, minimal diff dari upstream
- ✅ Bisa diterapkan via environment variable
- ❌ Tidak bisa memanfaatkan ARM-specific extensions
- ❌ Tidak bisa optimize shader compilation untuk Valhall
- ❌ Tidak bisa handle tiler-specific quirks di kode

**Verdict**: **Ditolak** - Terlalu terbatas untuk "comprehensive optimization"

---

### Approach 2: Feature-Gated Optimizations (Recommended)
**Description**: Menambahkan deteksi GPU Mali (vendor ID 0x13B5 = ARM) dan mengaktifkan optimasi secara kondisional:
- Dynamic rendering sebagai default
- FP16/INT8 shader paths
- Descriptor indexing / bindless
- ARM-specific builtins untuk shader intrinsics
- Tiler-aware memory dan render pass handling
- Shader tile image untuk tile memory access

**Trade-offs**:
- ✅ Memanfaatkan semua kemampuan Mali-G615
- ✅ Backward compatible dengan GPU lain
- ✅ Bisa di-enable/disable via config
- ⚌ Perlu testing ekstensif per game
- ⚌ Maintenance burden medium

**Verdict**: **Direkomendasikan** - Best balance

---

### Approach 3: Full Mali-Specific Backend
**Description**: Membuat code path terpisah sepenuhnya untuk ARM Mali (mirip RADV vs AMDVLK).

**Trade-offs**:
- ✅ Optimasi maksimal
- ✅ Bisa gunakan fitur proprietary ARM
- ❌ Maintenance sangat tinggi
- ❌ Fragmentasi kode besar
- ❌ Sulit merge upstream changes

**Verdict**: **Ditolak** - Over-engineering untuk proyek ini

---

## Selected Approach: Feature-Gated Optimizations (Approach 2)

### Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| **Deteksi Mali via `VK_DRIVER_ID_ARM_PROPRIETARY`** | Sudah ada di `dxvk_device.cpp:703`, extend untuk features |
| **Dynamic Rendering default ON** | Mali supports VK_KHR_dynamic_rendering, eliminates render pass overhead |
| **FP16/INT8 shader compilation** | Mali Valhall has native FP16/INT8 ALU, 2x throughput vs FP32 |
| **Descriptor Indexing + Bindless** | Reduces descriptor set binding overhead, critical for draw call heavy games |
| **Buffer Device Address** | Lower overhead than descriptor buffers for buffer access |
| **Subgroup ops (quad/ballot/shuffle)** | Mali supports subgroupSizeControl, wave64 native |
| **Shader Tile Image (VK_EXT_shader_tile_image)** | Valhall tile memory access for render pass load/store optimization |
| **ARM Shader Core Builtins** | Access to core ID, lane ID, quad ops for custom optimizations |
| **Pipeline Creation Cache Control** | Reduce shader compile stutter |
| **Synchronization2** | Fine-grained barriers, less CPU overhead |

---

## Implementation Plan

### Phase 1: Core Device Detection & Config (Week 1)
1. Extend `DxvkDevice::detectMaliFeatures()` - deteksi spesifik Mali-G615
2. Tambah config options di `dxvk_options.h/cpp`:
   - `dxvk.enableDynamicRendering` (default: true untuk Mali)
   - `dxvk.enableFP16Shaders` (default: true untuk Mali)
   - `dxvk.enableDescriptorIndexing` (default: true untuk Mali)
   - `dxvk.enableBufferDeviceAddress` (default: true untuk Mali)
   - `dxvk.enableShaderTileImage` (default: true untuk Mali)
   - `dxvk.enableARMBuiltins` (default: true untuk Mali)
3. Update `VP_DXVK_requirements.json` dengan Mali-specific requirements

### Phase 2: Render Pass & Dynamic Rendering (Week 1-2)
1. Modifikasi `dxvk_renderpass.cpp/h` - prioritaskan dynamic rendering
2. Update `dxvk_graphics.cpp` - gunakan `VK_KHR_dynamic_rendering` saat memungkinkan
3. Handle tiler-specific: `preferRenderPassOps = true` untuk Mali (sudah ada)

### Phase 3: Shader Compilation Optimizations (Week 2-3)
1. `dxvk_shader_spirv.cpp` - generate FP16/INT8 ops saat `shaderFloat16`/`shaderInt8` tersedia
2. `dxvk_shader_ir.cpp` - optimize IR untuk Valhall (lower precision, subgroup ops)
3. `dxvk_pipemanager.cpp` - enable Graphics Pipeline Library untuk pre-compile
4. Shader cache: kompresi cache dengan SPIR-V remapping untuk Mali

### Phase 4: Descriptor & Memory Management (Week 3)
1. `dxvk_descriptor_pool.cpp` - gunakan descriptor indexing untuk bindless
2. `dxvk_memory.cpp` - optimize untuk unified memory (cached vs uncached heuristics)
3. `dxvk_buffer.cpp` - gunakan buffer device address untuk SSBO/UBO
4. Push constant optimization (max 256 bytes di Mali)

### Phase 5: ARM-Specific Extensions (Week 3-4)
1. `VK_ARM_shader_core_builtins` - inject core/lane/quad intrinsics
2. `VK_ARM_rasterization_order_attachment_access` - untuk order-independent transparency
3. `VK_EXT_shader_tile_image` - tile memory load/store di fragment shader
4. `VK_KHR_fragment_shading_rate` - variable rate shading support

### Phase 6: Synchronization & Pipeline (Week 4)
1. `dxvk_barrier.cpp` - migrasi ke Synchronization2
2. Timeline semaphore untuk CPU-GPU sync
3. Pipeline cache serialization yang efisien

### Phase 7: Android/Termux Integration (Week 4-5)
1. Build script untuk cross-compile Windows DLL (MinGW-w64)
2. Build script untuk native Linux .so (Android/Termux)
3. WSI backend untuk Android (ANativeWindow via SDL3)
4. Integration testing dengan Winlator/Box64

### Phase 8: Testing & Profiling (Week 5-6)
1. Test suite: D3D9/11 conformance
2. Game testing: AAA titles via Winlator
3. Profiling: GPU counters via `DXVK_HUD=gpuload,fps,frametimes`
4. Regression testing vs upstream DXVK

---

## Configuration Preset for Mali-G615

```ini
# dxvk.conf - Mali-G615 Optimal Preset
[dxvk]
# Rendering
enableDynamicRendering = True
enableGraphicsPipelineLibrary = True

# Shaders
enableFP16Shaders = True
enableINT8Shaders = True
enableDescriptorIndexing = True
enableBufferDeviceAddress = True
enableShaderTileImage = True
enableARMBuiltins = True
lowerSinCos = Auto  # Mali has good sin/cos

# Memory (unified memory)
enableMemoryDefrag = True
maxMemoryBudget = 3072  # 3GB limit untuk Android
preferCachedMemory = True  # Tiler preference

# Synchronization
enableSynchronization2 = True
enableTimelineSemaphore = True

# Pipeline
enableDescriptorHeap = Auto
enableDescriptorBuffer = False  # Not stable yet
pipelineCreationCacheControl = True

# Tiler
tilerMode = True
preferRenderPassOps = True

# HUD for profiling
hud = devinfo,fps,frametimes,gpuload,compiler,drawcalls,memory
```

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Driver bugs di ekstensi baru | Feature gate per extension, fallback ke path standar |
| Shader compile stutter | Graphics Pipeline Library + async compile + persistent cache |
| Memory pressure (unified) | Aggressive memory defrag, budget limiting, cached allocations |
| Box64/Wine compatibility | Test setiap PR, CI dengan game populer |
| Upstream merge conflicts | Minimal invasive changes, feature flags, clear separation |

---

## Success Metrics

| Metric | Target |
|--------|--------|
| FPS improvement vs upstream | +20-40% |
| Shader compile stutter | < 16ms (1 frame) |
| 99th percentile frame time | < 33ms (30 FPS) |
| Memory usage | < 3GB untuk game AAA |
| Crash rate | < 0.1% sessions |
| Game compatibility | > 90% tested titles |

---

## What's Left to Builder

1. **Implementation details**: Exact SPIR-V patterns untuk FP16/INT8/Subgroup
2. **Game-specific workarounds**: Akan ditemukan selama testing
3. **Driver version-specific quirks**: Mali driver r44p1 vs r45+
4. **Android API level requirements**: Minimum API untuk setiap fitur
5. **Box64 integration details**: Wine/Box64 DXVK loading mechanism

---

## Visual: Optimization Areas

```
┌─────────────────────────────────────────────────────────────┐
│                    DXVM Mali-G615 Stack                     │
├─────────────────────────────────────────────────────────────┤
│  Application (D3D8/9/10/11 via Wine/Box64)                 │
├─────────────────────────────────────────────────────────────┤
│  DXVM Translation Layer                                     │
│  ┌──────────────┬──────────────┬────────────────────────┐  │
│  │  Pipeline    │  Memory      │  Synchronization       │  │
│  │  • DynRender │  • Unified   │  • Sync2               │  │
│  │  • GPL       │  • BDA       │  • Timeline Sem        │  │
│  │  • Tile Image│  • Defrag    │  • Tiler-aware         │  │
│  └──────────────┴──────────────┴────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│  Shader Compiler                                            │
│  ┌──────────────┬──────────────┬────────────────────────┐  │
│  │  IR Opt      │  Codegen     │  Cache                 │  │
│  │  • FP16/INT8 │  • Subgroup  │  • Persistent          │  │
│  │  • Valhall   │  • ARM       │  • Compressed          │  │
│  │    patterns  │    Builtins  │  • GPL pre-compile     │  │
│  └──────────────┴──────────────┴────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│  Vulkan 1.3 + ARM Extensions (Mali Driver r44p1+)          │
└─────────────────────────────────────────────────────────────┘
```

---

## Approval

This design document covers the comprehensive optimization approach for DXVM on Mali-G615 MC2. The feature-gated approach (Approach 2) balances performance gains with maintainability and compatibility.

**Next Step**: Upon approval, invoke `writing-plans` skill to create detailed implementation plan with tasks and dependencies.