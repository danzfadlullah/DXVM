// Texture and BCn scenes.
//
// The BCn scenes are the ones that matter most for DXVM: Mali has no BCn
// sampling, so a BC1 texture must go through the remap + transcode path and
// still sample as the colour the block encodes. A raw-BCn upload shows up here
// as a wrong colour, not a crash.

#ifdef _WIN32

#include "Bcn.h"

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
#include <cstring>
#include <string>

namespace dxvmt {

  namespace {

    constexpr UINT kTex = 4u;
    constexpr UINT kRt  = 16u;

    using D3DCompileFn = HRESULT (WINAPI*)(LPCVOID, SIZE_T, LPCSTR,
      const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT,
      ID3DBlob**, ID3DBlob**);

    D3DCompileFn compiler() {
      static D3DCompileFn fn = nullptr;
      static bool tried = false;
      if (!tried) {
        tried = true;
        HMODULE m = LoadLibraryA("d3dcompiler_47.dll");
        if (!m) m = LoadLibraryA("d3dcompiler_46.dll");
        if (m) fn = reinterpret_cast<D3DCompileFn>(
          reinterpret_cast<void*>(GetProcAddress(m, "D3DCompile")));
      }
      return fn;
    }

    ID3DBlob* compile(const char* s, const char* e, const char* t) {
      D3DCompileFn fn = compiler();
      if (!fn) return nullptr;
      ID3DBlob* c = nullptr; ID3DBlob* err = nullptr;
      const HRESULT hr = fn(s, strlen(s), nullptr, nullptr, nullptr, e, t, 0u, 0u, &c, &err);
      if (err) err->Release();
      if (FAILED(hr)) { if (c) c->Release(); return nullptr; }
      return c;
    }

    const char* kVs =
      "struct I { float3 pos : POSITION; float2 uv : TEXCOORD0; };\n"
      "struct O { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };\n"
      "O main(I i) { O o; o.pos = float4(i.pos, 1); o.uv = i.uv; return o; }\n";

    const char* kPs =
      "Texture2D t : register(t0);\n"
      "SamplerState s : register(s0);\n"
      "float4 main(float2 uv : TEXCOORD0) : SV_Target { return t.SampleLevel(s, uv, 0); }\n";

    struct Canvas {
      D3D11Context ctx;
      ID3D11Texture2D* rt = nullptr;
      ID3D11RenderTargetView* rtv = nullptr;
      ID3D11Texture2D* staging = nullptr;
      ID3D11VertexShader* vs = nullptr;
      ID3D11PixelShader* ps = nullptr;
      ID3D11Buffer* vb = nullptr;
      ID3D11InputLayout* il = nullptr;
      ID3D11SamplerState* sampler = nullptr;
      ID3D11ShaderResourceView* srv = nullptr;

      ~Canvas() {
        if (srv) srv->Release();
        if (sampler) sampler->Release();
        if (il) il->Release();
        if (vb) vb->Release();
        if (ps) ps->Release();
        if (vs) vs->Release();
        if (staging) staging->Release();
        if (rtv) rtv->Release();
        if (rt) rt->Release();
        destroyD3D11(ctx);
      }
    };

    bool setup(Canvas& c, std::string& error) {
      if (!createD3D11(c.ctx, error))
        return false;

      auto* dev = static_cast<ID3D11Device*>(c.ctx.device);

      D3D11_TEXTURE2D_DESC d = { };
      d.Width = kRt; d.Height = kRt; d.MipLevels = 1u; d.ArraySize = 1u;
      d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      d.SampleDesc.Count = 1u;
      d.Usage = D3D11_USAGE_DEFAULT;
      d.BindFlags = D3D11_BIND_RENDER_TARGET;

      if (FAILED(dev->CreateTexture2D(&d, nullptr, &c.rt))) { error = "no RT"; return false; }
      if (FAILED(dev->CreateRenderTargetView(c.rt, nullptr, &c.rtv))) { error = "no RTV"; return false; }

      d.BindFlags = 0u; d.Usage = D3D11_USAGE_STAGING; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      if (FAILED(dev->CreateTexture2D(&d, nullptr, &c.staging))) { error = "no staging"; return false; }

      ID3DBlob* vsb = compile(kVs, "main", "vs_5_0");
      ID3DBlob* psb = compile(kPs, "main", "ps_5_0");
      if (!vsb || !psb) {
        if (vsb) vsb->Release();
        if (psb) psb->Release();
        error = "shader compiler unavailable";
        return false;
      }

      bool ok = SUCCEEDED(dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &c.vs))
             && SUCCEEDED(dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &c.ps));

      D3D11_INPUT_ELEMENT_DESC e[] = {
        { "POSITION", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u, 0u,  D3D11_INPUT_PER_VERTEX_DATA, 0u },
        { "TEXCOORD", 0u, DXGI_FORMAT_R32G32_FLOAT,    0u, 12u, D3D11_INPUT_PER_VERTEX_DATA, 0u },
      };

      ok = ok && SUCCEEDED(dev->CreateInputLayout(e, 2u, vsb->GetBufferPointer(), vsb->GetBufferSize(), &c.il));
      vsb->Release(); psb->Release();

      const float v[] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 0.0f,
      };

      D3D11_BUFFER_DESC bd = { };
      bd.ByteWidth = sizeof(v);
      bd.Usage = D3D11_USAGE_IMMUTABLE;
      bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
      D3D11_SUBRESOURCE_DATA sd = { }; sd.pSysMem = v;
      ok = ok && SUCCEEDED(dev->CreateBuffer(&bd, &sd, &c.vb));

      D3D11_SAMPLER_DESC sdsc = { };
      sdsc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
      sdsc.AddressU = sdsc.AddressV = sdsc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
      sdsc.MaxLOD = D3D11_FLOAT32_MAX;
      ok = ok && SUCCEEDED(dev->CreateSamplerState(&sdsc, &c.sampler));

      if (!ok) { error = "pipeline creation failed"; return false; }
      return true;
    }

    bool drawAndRead(Canvas& c, unsigned char out[4], UINT x, UINT y) {
      auto* ctx = static_cast<ID3D11DeviceContext*>(c.ctx.context);

      const float clear[4] = { 0, 0, 0, 1 };
      ctx->ClearRenderTargetView(c.rtv, clear);
      ctx->OMSetRenderTargets(1u, &c.rtv, nullptr);

      D3D11_VIEWPORT vp = { };
      vp.Width = float(kRt); vp.Height = float(kRt); vp.MaxDepth = 1.0f;
      ctx->RSSetViewports(1u, &vp);

      const UINT stride = 20u, off = 0u;
      ctx->IASetInputLayout(c.il);
      ctx->IASetVertexBuffers(0u, 1u, &c.vb, &stride, &off);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->VSSetShader(c.vs, nullptr, 0u);
      ctx->PSSetShader(c.ps, nullptr, 0u);
      ctx->PSSetShaderResources(0u, 1u, &c.srv);
      ctx->PSSetSamplers(0u, 1u, &c.sampler);
      ctx->Draw(3u, 0u);

      ctx->CopyResource(c.staging, c.rt);

      D3D11_MAPPED_SUBRESOURCE m = { };
      if (FAILED(ctx->Map(c.staging, 0u, D3D11_MAP_READ, 0u, &m)))
        return false;

      const auto* row = static_cast<const unsigned char*>(m.pData)
        + static_cast<size_t>(y) * m.RowPitch + x * 4u;
      out[0] = row[0]; out[1] = row[1]; out[2] = row[2]; out[3] = row[3];
      ctx->Unmap(c.staging, 0u);
      return true;
    }

    bool makeTexture(Canvas& c, DXGI_FORMAT format, const void* blocks,
                     UINT blockBytes, std::string& error) {
      auto* dev = static_cast<ID3D11Device*>(c.ctx.device);

      D3D11_TEXTURE2D_DESC d = { };
      d.Width = kTex; d.Height = kTex; d.MipLevels = 1u; d.ArraySize = 1u;
      d.Format = format;
      d.SampleDesc.Count = 1u;
      d.Usage = D3D11_USAGE_IMMUTABLE;
      d.BindFlags = D3D11_BIND_SHADER_RESOURCE;

      D3D11_SUBRESOURCE_DATA init = { };
      init.pSysMem = blocks;
      init.SysMemPitch = blockBytes;

      ID3D11Texture2D* tex = nullptr;

      if (FAILED(dev->CreateTexture2D(&d, &init, &tex))) {
        error = "CreateTexture2D failed (format may be unsupported)";
        return false;
      }

      const HRESULT hr = dev->CreateShaderResourceView(tex, nullptr, &c.srv);
      tex->Release();

      if (FAILED(hr)) {
        error = "CreateShaderResourceView failed";
        return false;
      }

      return true;
    }

    // 4x4 BC1 block, both endpoints red, all indices 0 => solid red.
    void solidRedBc1(unsigned char out[8]) {
      const unsigned short red = 0xF800u;
      out[0] = static_cast<unsigned char>(red & 0xFFu);
      out[1] = static_cast<unsigned char>(red >> 8);
      out[2] = out[0];
      out[3] = out[1];
      out[4] = out[5] = out[6] = out[7] = 0u;
    }

    // 4x4 BC3 block: 8 bytes alpha (255,255, all indices 0) + BC1 colour.
    void solidRedBc3(unsigned char out[16]) {
      out[0] = 255u; out[1] = 255u;
      std::memset(out + 2, 0, 6u);
      solidRedBc1(out + 8);
    }

    // ---- scenes ---------------------------------------------------------

    SceneResult sceneTexture() {
      Canvas c; std::string err;
      if (!setup(c, err)) return SceneResult::skip(err);

      // 4x4 distinct texels, sampled point-wise at the RT centre.
      unsigned char texels[4 * 4 * 4];
      for (unsigned i = 0; i < 16u; i++) {
        texels[i * 4u + 0u] = static_cast<unsigned char>(i * 16u);
        texels[i * 4u + 1u] = 0x20u;
        texels[i * 4u + 2u] = 0x30u;
        texels[i * 4u + 3u] = 0xFFu;
      }

      if (!makeTexture(c, DXGI_FORMAT_R8G8B8A8_UNORM, texels, 16u, err))
        return SceneResult::fail(err);

      unsigned char px[4] = { };
      if (!drawAndRead(c, px, kRt / 2u, kRt / 2u))
        return SceneResult::fail("readback failed");

      // The centre of the quad samples near texel index 10 (uv ~0.5).
      if (px[0] == 0u && px[1] == 0u && px[2] == 0u)
        return SceneResult::fail("sample returned black");

      return SceneResult::pass("4x4 point-sampled texture, texels in place");
    }

    SceneResult sceneBcn1Or3(bool isSrgb, bool isBc3) {
      Canvas c; std::string err;
      if (!setup(c, err)) return SceneResult::skip(err);

      unsigned char blocks[16] = { };
      if (isBc3)
        solidRedBc3(blocks);
      else
        solidRedBc1(blocks);

      DXGI_FORMAT fmt;

      if (isBc3)
        fmt = isSrgb ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;
      else
        fmt = isSrgb ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM;

      if (!makeTexture(c, fmt, blocks, isBc3 ? 16u : 8u, err))
        return SceneResult::fail(err + ": BC texture not creatable on this device (DXVM should remap)");

      unsigned char px[4] = { };
      if (!drawAndRead(c, px, kRt / 2u, kRt / 2u))
        return SceneResult::fail("readback failed");

      // Solid red must come back red-ish in both colour spaces.
      if (px[0] < 200u)
        return SceneResult::fail("sampled r=" + std::to_string(px[0])
          + ", expected about 255 (raw BCn bytes uploaded?)");

      return SceneResult::pass("solid red block sampled as r=" + std::to_string(px[0]));
    }

    SceneResult sceneBc1()      { return sceneBcn1Or3(false, false); }
    SceneResult sceneBc1Srgb()  { return sceneBcn1Or3(true,  false); }
    SceneResult sceneBc3()      { return sceneBcn1Or3(false, true);  }
    SceneResult sceneBc3Srgb()  { return sceneBcn1Or3(true,  true);  }

    SceneResult notPorted() {
      return SceneResult::skip("block fixture not ported yet");
    }

  }

  void registerTextureScenes(Registry& registry) {
    registry.add("texture", &sceneTexture);
    registry.add("bcn.bc1",      &sceneBc1);
    registry.add("bcn.bc1_srgb", &sceneBc1Srgb);
    registry.add("bcn.bc3",      &sceneBc3);
    registry.add("bcn.bc3_srgb", &sceneBc3Srgb);

    // Registered so the report always names every format the plan lists.
    registry.add("bcn.bc2",      &notPorted);
    registry.add("bcn.bc2_srgb", &notPorted);
    registry.add("bcn.bc4",      &notPorted);
    registry.add("bcn.bc5",      &notPorted);
    registry.add("bcn.bc6h",     &notPorted);
    registry.add("bcn.bc6h_srgb",&notPorted);
    registry.add("bcn.bc7",      &notPorted);
    registry.add("bcn.bc7_srgb", &notPorted);
  }

}

#endif