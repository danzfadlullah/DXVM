// MSAA scenes.
//
// Three kinds:
//   msaacounts        — reported sample counts vs actually creatable (shader free)
//   msaa.coverage     — Nx MSAA triangle, resolve, count partial-coverage pixels
//   msaa.sampleindex  — resolved red equals the CPU mean of per-sample values
//   msaa.reference    — print DXML's pinned values for side-by-side comparison
//
// Windows-only.

#ifdef _WIN32

#include "Msaa.h"

#include "../Device.h"
#include "../Registry.h"
#include "../Report.h"

#ifndef NOMINMAX
  #define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include <cstdio>
#include <string>
#include <vector>

namespace dxvmt {

  namespace {

    // ---- runtime shader compilation -------------------------------------
    //
    // DXVM ships no d3dcompiler DLL; the host (Wine or Windows) provides one.
    // A missing compiler is reported as SKIP, never as PASS.

    using D3DCompileFn = HRESULT (WINAPI*)(LPCVOID, SIZE_T, LPCSTR,
      const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT,
      ID3DBlob**, ID3DBlob**);

    D3DCompileFn loadCompiler() {
      static D3DCompileFn fn = nullptr;
      static bool tried = false;

      if (!tried) {
        tried = true;
        HMODULE mod = LoadLibraryA("d3dcompiler_47.dll");
        if (!mod)
          mod = LoadLibraryA("d3dcompiler_46.dll");
        if (mod)
          fn = reinterpret_cast<D3DCompileFn>(
            reinterpret_cast<void*>(GetProcAddress(mod, "D3DCompile")));
      }

      return fn;
    }

    ID3DBlob* compile(const char* source, const char* entry, const char* target) {
      D3DCompileFn fn = loadCompiler();
      if (!fn)
        return nullptr;

      ID3DBlob* code = nullptr;
      ID3DBlob* errors = nullptr;

      const HRESULT hr = fn(source, strlen(source), nullptr, nullptr, nullptr,
        entry, target, 0u, 0u, &code, &errors);

      if (errors)
        errors->Release();

      if (FAILED(hr)) {
        if (code)
          code->Release();
        return nullptr;
      }

      return code;
    }

    // ---- shared setup ---------------------------------------------------

    constexpr UINT kWidth  = 64u;
    constexpr UINT kHeight = 64u;

    struct RenderContext {
      D3D11Context        ctx;
      ID3D11Texture2D*    msaaTarget = nullptr;
      ID3D11RenderTargetView* msaaRtv = nullptr;
      ID3D11Texture2D*    resolve = nullptr;
      ID3D11Texture2D*    readback = nullptr;
      ID3D11VertexShader* vs = nullptr;
      ID3D11PixelShader*  ps = nullptr;
      ID3D11Buffer*       vb = nullptr;
      ID3D11InputLayout*  layout = nullptr;

      ~RenderContext() {
        if (layout)     layout->Release();
        if (vb)         vb->Release();
        if (ps)         ps->Release();
        if (vs)         vs->Release();
        if (readback)   readback->Release();
        if (resolve)    resolve->Release();
        if (msaaRtv)    msaaRtv->Release();
        if (msaaTarget) msaaTarget->Release();
        destroyD3D11(ctx);
      }
    };

    // A triangle covering the left half plus a diagonal edge, so the resolve
    // produces pixels with partial sample coverage.
    const char* kVertexShader =
      "struct VSIn { float3 pos : POSITION; float4 col : COLOR; };\n"
      "struct VSOut { float4 pos : SV_POSITION; float4 col : COLOR; };\n"
      "VSOut main(VSIn i) { VSOut o; o.pos = float4(i.pos, 1.0); o.col = i.col; return o; }\n";

    const char* kCoverageShader =
      "float4 main(float4 col : COLOR) : SV_Target { return col; }\n";

    // Per-sample value: red carries the sample index scaled to 0..255 so a
    // wrong index mapping changes the resolved average.
    const char* kSampleIndexShader =
      "float4 main(float4 col : COLOR, uint idx : SV_SampleIndex) : SV_Target {\n"
      "  float v = (float)idx * (255.0 / 15.0);\n"
      "  return float4(v, 0, 0, 1);\n"
      "}\n";

    bool createMsaaTargets(RenderContext& rc, UINT sampleCount, DXGI_FORMAT format) {
      auto* device = static_cast<ID3D11Device*>(rc.ctx.device);

      D3D11_TEXTURE2D_DESC desc = { };
      desc.Width      = kWidth;
      desc.Height     = kHeight;
      desc.MipLevels  = 1u;
      desc.ArraySize  = 1u;
      desc.Format     = format;
      desc.SampleDesc.Count = sampleCount;
      desc.Usage      = D3D11_USAGE_DEFAULT;
      desc.BindFlags  = D3D11_BIND_RENDER_TARGET;

      if (FAILED(device->CreateTexture2D(&desc, nullptr, &rc.msaaTarget)))
        return false;

      if (FAILED(device->CreateRenderTargetView(rc.msaaTarget, nullptr, &rc.msaaRtv)))
        return false;

      D3D11_TEXTURE2D_DESC rdesc = desc;
      rdesc.SampleDesc.Count   = 1u;
      rdesc.SampleDesc.Quality = 0u;
      rdesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
      rdesc.Usage              = D3D11_USAGE_DEFAULT;

      if (FAILED(device->CreateTexture2D(&rdesc, nullptr, &rc.resolve)))
        return false;

      rdesc.BindFlags = 0u;
      rdesc.Usage     = D3D11_USAGE_STAGING;
      rdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

      if (FAILED(device->CreateTexture2D(&rdesc, nullptr, &rc.readback)))
        return false;

      return true;
    }

    bool createPipeline(RenderContext& rc, const char* pixelSource) {
      auto* device = static_cast<ID3D11Device*>(rc.ctx.device);

      ID3DBlob* vsBlob = compile(kVertexShader, "main", "vs_5_0");
      ID3DBlob* psBlob = compile(pixelSource, "main", "ps_5_0");

      if (!vsBlob || !psBlob) {
        if (vsBlob) vsBlob->Release();
        if (psBlob) psBlob->Release();
        return false;
      }

      HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, &rc.vs);
      hr = FAILED(hr) ? hr : device->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, &rc.ps);

      vsBlob->Release();
      psBlob->Release();

      if (FAILED(hr))
        return false;

      D3D11_INPUT_ELEMENT_DESC elems[] = {
        { "POSITION", 0u, DXGI_FORMAT_R32G32B32_FLOAT,    0u, 0u,  D3D11_INPUT_PER_VERTEX_DATA, 0u },
        { "COLOR",    0u, DXGI_FORMAT_R32G32B32A32_FLOAT, 0u, 12u, D3D11_INPUT_PER_VERTEX_DATA, 0u },
      };

      if (FAILED(device->CreateInputLayout(elems, 2u,
          vsBlob ? vsBlob->GetBufferPointer() : nullptr, 0u, &rc.layout))) {
        // Recreate from a fresh blob: the original was released above.
        ID3DBlob* again = compile(kVertexShader, "main", "vs_5_0");
        if (!again)
          return false;
        const HRESULT hr2 = device->CreateInputLayout(elems, 2u,
          again->GetBufferPointer(), again->GetBufferSize(), &rc.layout);
        again->Release();
        if (FAILED(hr2))
          return false;
      }

      const float verts[] = {
        // x, y, z,        r, g, b, a
        -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
         0.2f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
         0.2f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
      };

      D3D11_BUFFER_DESC bd = { };
      bd.ByteWidth = sizeof(verts);
      bd.Usage     = D3D11_USAGE_IMMUTABLE;
      bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

      D3D11_SUBRESOURCE_DATA init = { };
      init.pSysMem = verts;

      return SUCCEEDED(device->CreateBuffer(&bd, &init, &rc.vb));
    }

    bool render(RenderContext& rc) {
      auto* context = static_cast<ID3D11DeviceContext*>(rc.ctx.context);

      const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
      context->ClearRenderTargetView(rc.msaaRtv, clear);

      const UINT stride = 28u, offset = 0u;
      context->IASetInputLayout(rc.layout);
      context->IASetVertexBuffers(0u, 1u, &rc.vb, &stride, &offset);
      context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      context->VSSetShader(rc.vs, nullptr, 0u);
      context->PSSetShader(rc.ps, nullptr, 0u);
      context->OMSetRenderTargets(1u, &rc.msaaRtv, nullptr);

      D3D11_VIEWPORT vp = { };
      vp.Width  = static_cast<float>(kWidth);
      vp.Height = static_cast<float>(kHeight);
      vp.MaxDepth = 1.0f;
      context->RSSetViewports(1u, &vp);
      context->Draw(3u, 0u);

      context->ResolveSubresource(rc.resolve, 0u, rc.msaaTarget, 0u,
        DXGI_FORMAT_R8G8B8A8_UNORM);

      context->CopyResource(rc.readback, rc.resolve);
      return true;
    }

    struct Pixel { unsigned char r, g, b, a; };

    bool mapAndRead(RenderContext& rc, Pixel& out, UINT x, UINT y) {
      auto* context = static_cast<ID3D11DeviceContext*>(rc.ctx.context);

      D3D11_MAPPED_SUBRESOURCE mapped = { };
      if (FAILED(context->Map(rc.readback, 0u, D3D11_MAP_READ, 0u, &mapped)))
        return false;

      const auto* row = static_cast<const unsigned char*>(mapped.pData)
        + static_cast<size_t>(y) * mapped.RowPitch;
      const auto* px = reinterpret_cast<const Pixel*>(row) + x;

      out = *px;
      context->Unmap(rc.readback, 0u);
      return true;
    }

    /** Count pixels whose red is strictly between background and foreground. */
    int countPartialPixels(RenderContext& rc) {
      auto* context = static_cast<ID3D11DeviceContext*>(rc.ctx.context);

      D3D11_MAPPED_SUBRESOURCE mapped = { };
      if (FAILED(context->Map(rc.readback, 0u, D3D11_MAP_READ, 0u, &mapped)))
        return -1;

      int partial = 0;

      for (UINT y = 0; y < kHeight; y++) {
        const auto* row = static_cast<const unsigned char*>(mapped.pData)
          + static_cast<size_t>(y) * mapped.RowPitch;
        const auto* px = reinterpret_cast<const unsigned char*>(row);

        for (UINT x = 0; x < kWidth; x++) {
          const unsigned char r = px[x * 4u];
          if (r > 0u && r < 255u)
            partial++;
        }
      }

      context->Unmap(rc.readback, 0u);
      return partial;
    }

    // ---- scenes ---------------------------------------------------------

    SceneResult sceneMsaaCounts() {
      D3D11Context ctx;
      std::string error;

      if (!createD3D11(ctx, error))
        return SceneResult::fail(error);

      auto* device = static_cast<ID3D11Device*>(ctx.device);

      std::string detail;
      std::string broken;

      for (UINT count : { 2u, 4u, 8u, 16u }) {
        UINT quality = 0u;
        const HRESULT hr = device->CheckMultisampleQualityLevels(
          DXGI_FORMAT_R8G8B8A8_UNORM, count, &quality);

        if (FAILED(hr)) {
          char buf[96];
          std::snprintf(buf, sizeof(buf),
            "CheckMultisampleQualityLevels failed 0x%08lx",
            static_cast<unsigned long>(hr));
          if (!broken.empty()) broken += "; ";
          broken += buf;
          continue;
        }

        bool created = false;

        if (quality > 0u) {
          // A count the device claims must also be creatable.
          D3D11_TEXTURE2D_DESC desc = { };
          desc.Width      = kWidth;
          desc.Height     = kHeight;
          desc.MipLevels  = 1u;
          desc.ArraySize  = 1u;
          desc.Format     = DXGI_FORMAT_R8G8B8A8_UNORM;
          desc.SampleDesc.Count = count;
          desc.Usage      = D3D11_USAGE_DEFAULT;
          desc.BindFlags  = D3D11_BIND_RENDER_TARGET;

          ID3D11Texture2D* tex = nullptr;
          created = SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &tex));
          if (tex)
            tex->Release();
        }

        if (quality > 0u && !created) {
          if (!broken.empty()) broken += "; ";
          broken += std::to_string(count) + "x reported but not creatable";
        }

        if (!detail.empty()) detail += ", ";
        detail += std::to_string(count) + "x=" + (quality > 0u ? "yes" : "no");
      }

      destroyD3D11(ctx);

      if (!broken.empty())
        return SceneResult::fail(broken);

      return SceneResult::pass("reported, created: " + detail);
    }

    SceneResult sceneMsaaCoverage() {
      RenderContext rc;
      std::string error;

      if (!createD3D11(rc.ctx, error))
        return SceneResult::fail(error);

      if (!createMsaaTargets(rc, 4u, DXGI_FORMAT_R8G8B8A8_UNORM))
        return SceneResult::skip("4x MSAA not supported");

      if (!createPipeline(rc, kCoverageShader))
        return SceneResult::skip("shader compiler unavailable");

      if (!render(rc))
        return SceneResult::fail("render or resolve failed");

      const int partial = countPartialPixels(rc);
      const int want    = msaaPartialPixelExpectation();

      if (partial < 0)
        return SceneResult::fail("readback map failed");

      if (partial != want) {
        return SceneResult::fail(
          "4x MSAA resolve, partial coverage on the edge: "
          + std::to_string(partial) + " partial pixels, expected " + std::to_string(want));
      }

      return SceneResult::pass("4x MSAA triangle resolved, diagonal edge has partial coverage");
    }

    SceneResult sceneMsaaSampleIndex() {
      RenderContext rc;
      std::string error;

      if (!createD3D11(rc.ctx, error))
        return SceneResult::fail(error);

      const UINT count = 4u;

      if (!createMsaaTargets(rc, count, DXGI_FORMAT_R8G8B8A8_UNORM))
        return SceneResult::skip("4x MSAA not supported");

      if (!createPipeline(rc, kSampleIndexShader))
        return SceneResult::skip("shader compiler unavailable");

      if (!render(rc))
        return SceneResult::fail("render or resolve failed");

      Pixel px = { };
      if (!mapAndRead(rc, px, 10u, 20u))
        return SceneResult::fail("readback map failed");

      // Fully covered interior pixel: the resolve averages the per-sample
      // values over all four samples. With value = idx * 255/15 the mean is
      // (0+1+2+3)/4 * 255/15 = 25.5, so 25 or 26 depending on rounding. A
      // device that reports the same index for every sample yields 0 or 255.
      const int want = 25;
      const int got  = static_cast<int>(px.r);
      const bool ok  = got == want || got == want + 1;

      if (!ok) {
        return SceneResult::fail("red is " + std::to_string(got)
          + ", expected " + std::to_string(want)
          + " (same index for every sample?)");
      }

      return SceneResult::pass("resolved red is " + std::to_string(got)
        + ", expected " + std::to_string(want));
    }

    SceneResult sceneMsaaReference() {
      std::string detail;
      for (int count : { 2, 4, 8, 16 }) {
        if (!detail.empty())
          detail += ", ";
        detail += std::to_string(count) + "x=" + std::to_string(expectedSampleIndexRamp(count));
      }
      return SceneResult::pass("DXML reference sampleindex " + detail);
    }

  }

  void registerMsaaScenes(Registry& registry) {
    registry.add("msaacounts",       &sceneMsaaCounts);
    registry.add("msaa.coverage",    &sceneMsaaCoverage);
    registry.add("msaa.sampleindex", &sceneMsaaSampleIndex);
    registry.add("msaa.reference",   &sceneMsaaReference);
  }

}

#endif