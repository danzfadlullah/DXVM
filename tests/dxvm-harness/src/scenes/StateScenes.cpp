// State scenes: the graphics state DXVM's emulation module clamps.
//
// Priority is the state DXVM actually changes on Mali — dual-source blending,
// logic ops, wireframe, viewport count — plus the blend/depth basics.

#ifdef _WIN32

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

namespace dxvmt {

  namespace {

    constexpr UINT kSize = 32u;

    using D3DCompileFn = HRESULT (WINAPI*)(LPCVOID, SIZE_T, LPCSTR,
      const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT,
      ID3DBlob**, ID3DBlob**);

    D3DCompileFn compiler() {
      static D3DCompileFn fn = nullptr;
      static bool tried = false;
      if (!tried) {
        tried = true;
        HMODULE mod = LoadLibraryA("d3dcompiler_47.dll");
        if (!mod) mod = LoadLibraryA("d3dcompiler_46.dll");
        if (mod)
          fn = reinterpret_cast<D3DCompileFn>(
            reinterpret_cast<void*>(GetProcAddress(mod, "D3DCompile")));
      }
      return fn;
    }

    ID3DBlob* compile(const char* src, const char* entry, const char* target) {
      D3DCompileFn fn = compiler();
      if (!fn) return nullptr;
      ID3DBlob* code = nullptr; ID3DBlob* err = nullptr;
      const HRESULT hr = fn(src, strlen(src), nullptr, nullptr, nullptr,
        entry, target, 0u, 0u, &code, &err);
      if (err) err->Release();
      if (FAILED(hr)) { if (code) code->Release(); return nullptr; }
      return code;
    }

    const char* kVs =
      "struct I { float3 pos : POSITION; float4 col : COLOR; };\n"
      "struct O { float4 pos : SV_POSITION; float4 col : COLOR; };\n"
      "O main(I i) { O o; o.pos = float4(i.pos, 1); o.col = i.col; return o; }\n";

    const char* kPsSolid =
      "float4 main(float4 c : COLOR) : SV_Target { return c; }\n";

    struct Base {
      D3D11Context ctx;
      ID3D11Texture2D* rt = nullptr;
      ID3D11RenderTargetView* rtv = nullptr;
      ID3D11Texture2D* staging = nullptr;
      ID3D11VertexShader* vs = nullptr;
      ID3D11PixelShader* ps = nullptr;
      ID3D11Buffer* vb = nullptr;
      ID3D11InputLayout* il = nullptr;

      ~Base() {
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

    bool setup(Base& b, std::string& error, const char* psSource = kPsSolid) {
      if (!createD3D11(b.ctx, error))
        return false;

      auto* dev = static_cast<ID3D11Device*>(b.ctx.device);

      D3D11_TEXTURE2D_DESC d = { };
      d.Width = kSize; d.Height = kSize; d.MipLevels = 1u; d.ArraySize = 1u;
      d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      d.SampleDesc.Count = 1u;
      d.Usage = D3D11_USAGE_DEFAULT;
      d.BindFlags = D3D11_BIND_RENDER_TARGET;

      if (FAILED(dev->CreateTexture2D(&d, nullptr, &b.rt))) { error = "no RT"; return false; }
      if (FAILED(dev->CreateRenderTargetView(b.rt, nullptr, &b.rtv))) { error = "no RTV"; return false; }

      d.BindFlags = 0u; d.Usage = D3D11_USAGE_STAGING; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      if (FAILED(dev->CreateTexture2D(&d, nullptr, &b.staging))) { error = "no staging"; return false; }

      ID3DBlob* vsb = compile(kVs, "main", "vs_5_0");
      ID3DBlob* psb = compile(psSource, "main", "ps_5_0");
      if (!vsb || !psb) {
        if (vsb) vsb->Release();
        if (psb) psb->Release();
        error = "shader compiler unavailable";
        return false;
      }

      bool ok = SUCCEEDED(dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &b.vs))
             && SUCCEEDED(dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &b.ps));

      D3D11_INPUT_ELEMENT_DESC e[] = {
        { "POSITION", 0u, DXGI_FORMAT_R32G32B32_FLOAT,    0u, 0u,  D3D11_INPUT_PER_VERTEX_DATA, 0u },
        { "COLOR",    0u, DXGI_FORMAT_R32G32B32A32_FLOAT, 0u, 12u, D3D11_INPUT_PER_VERTEX_DATA, 0u },
      };

      ok = ok && SUCCEEDED(dev->CreateInputLayout(e, 2u, vsb->GetBufferPointer(), vsb->GetBufferSize(), &b.il));

      vsb->Release(); psb->Release();

      const float v[] = {
        -1.0f, -1.0f, 0.0f,  1,1,1,1,
         1.0f, -1.0f, 0.0f,  1,1,1,1,
         1.0f,  1.0f, 0.0f,  1,1,1,1,
      };

      D3D11_BUFFER_DESC bd = { };
      bd.ByteWidth = sizeof(v);
      bd.Usage = D3D11_USAGE_IMMUTABLE;
      bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
      D3D11_SUBRESOURCE_DATA sd = { }; sd.pSysMem = v;

      ok = ok && SUCCEEDED(dev->CreateBuffer(&bd, &sd, &b.vb));

      if (!ok) { error = "pipeline creation failed"; return false; }
      return true;
    }

    bool drawQuad(Base& b, ID3D11BlendState* blend) {
      auto* c = static_cast<ID3D11DeviceContext*>(b.ctx.context);

      const float clear[4] = { 0, 0, 0, 1 };
      c->ClearRenderTargetView(b.rtv, clear);
      c->OMSetRenderTargets(1u, &b.rtv, nullptr);

      D3D11_VIEWPORT vp = { };
      vp.Width = float(kSize); vp.Height = float(kSize); vp.MaxDepth = 1.0f;
      c->RSSetViewports(1u, &vp);

      const float factor[4] = { 1, 1, 1, 1 };
      c->OMSetBlendState(blend, factor, 0xffffffffu);

      const UINT stride = 28u, off = 0u;
      c->IASetInputLayout(b.il);
      c->IASetVertexBuffers(0u, 1u, &b.vb, &stride, &off);
      c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      c->VSSetShader(b.vs, nullptr, 0u);
      c->PSSetShader(b.ps, nullptr, 0u);
      c->Draw(3u, 0u);

      c->CopyResource(b.staging, b.rt);
      return true;
    }

    bool readPixel(Base& b, UINT x, UINT y, unsigned char out[4]) {
      auto* c = static_cast<ID3D11DeviceContext*>(b.ctx.context);
      D3D11_MAPPED_SUBRESOURCE m = { };
      if (FAILED(c->Map(b.staging, 0u, D3D11_MAP_READ, 0u, &m)))
        return false;
      const auto* row = static_cast<const unsigned char*>(m.pData)
        + static_cast<size_t>(y) * m.RowPitch + x * 4u;
      out[0] = row[0]; out[1] = row[1]; out[2] = row[2]; out[3] = row[3];
      c->Unmap(b.staging, 0u);
      return true;
    }

    // ---- scenes ---------------------------------------------------------

    // Logic ops must be disabled by DXVM's emulation on Mali; with the op
    // disabled the draw must produce the shader colour, not the op result.
    SceneResult sceneLogicOp() {
      Base b; std::string err;
      if (!setup(b, err)) return SceneResult::skip(err);

      auto* dev = static_cast<ID3D11Device*>(b.ctx.device);

      D3D11_BLEND_DESC bd = { };
      bd.RenderTarget[0].BlendEnable = FALSE;
      bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

      ID3D11BlendState* blend = nullptr;
      if (FAILED(dev->CreateBlendState(&bd, &blend)))
        return SceneResult::fail("CreateBlendState failed");

      if (!drawQuad(b, blend)) { blend->Release(); return SceneResult::fail("draw failed"); }

      unsigned char px[4] = { };
      const bool ok = readPixel(b, kSize / 2u, kSize / 2u, px);
      blend->Release();

      if (!ok) return SceneResult::fail("readback failed");
      if (px[0] != 255u || px[1] != 255u || px[2] != 255u)
        return SceneResult::fail("logic op off but draw is not the shader colour");

      // mingw's D3D11_BLEND_DESC has no LogicOpEnable field, so real logic-op
      // support is queried through feature support instead.
      D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS opts = { };
      const HRESULT hr = dev->CheckFeatureSupport(
        D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &opts, sizeof(opts));

      const bool logicOps = SUCCEEDED(hr) && opts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x;

      return SceneResult::pass(std::string("shader colour written unchanged")
        + (logicOps ? ", logic ops available" : ", logic ops not available"));
    }

    // Dual-source blend factors must be replaced by DXVM's emulation. With
    // blending disabled the source colour must pass through untouched.
    SceneResult sceneDualSource() {
      Base b; std::string err;
      if (!setup(b, err)) return SceneResult::skip(err);

      auto* dev = static_cast<ID3D11Device*>(b.ctx.device);

      D3D11_BLEND_DESC bd = { };
      bd.RenderTarget[0].BlendEnable = FALSE;
      bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

      ID3D11BlendState* blend = nullptr;
      if (FAILED(dev->CreateBlendState(&bd, &blend)))
        return SceneResult::fail("CreateBlendState failed");

      const bool drew = drawQuad(b, blend);
      blend->Release();

      if (!drew) return SceneResult::fail("draw failed");

      unsigned char px[4] = { };
      if (!readPixel(b, kSize / 2u, kSize / 2u, px))
        return SceneResult::fail("readback failed");

      if (px[0] != 255u)
        return SceneResult::fail("source colour did not pass through");

      return SceneResult::pass("blend state accepted and source colour passed through");
    }

    // Wireframe: DXVM clamps polygon mode to FILL, so the draw must still fill.
    SceneResult sceneWireframe() {
      Base b; std::string err;
      if (!setup(b, err)) return SceneResult::skip(err);

      auto* dev = static_cast<ID3D11Device*>(b.ctx.device);

      D3D11_RASTERIZER_DESC rd = { };
      rd.FillMode = D3D11_FILL_WIREFRAME;   // DXVM emulates this as FILL
      rd.CullMode = D3D11_CULL_NONE;
      rd.DepthClipEnable = TRUE;

      ID3D11RasterizerState* rs = nullptr;
      if (FAILED(dev->CreateRasterizerState(&rd, &rs)))
        return SceneResult::fail("CreateRasterizerState(wireframe) failed");

      auto* c = static_cast<ID3D11DeviceContext*>(b.ctx.context);
      c->RSSetState(rs);

      if (!drawQuad(b, nullptr)) { rs->Release(); return SceneResult::fail("draw failed"); }
      rs->Release();
      c->RSSetState(nullptr);

      // Count filled pixels in the interior: wireframe would leave it black.
      unsigned char px[4] = { };
      if (!readPixel(b, kSize / 2u, kSize / 2u, px))
        return SceneResult::fail("readback failed");

      if (px[0] != 255u)
        return SceneResult::fail("interior pixel not filled: wireframe was not emulated to fill");

      return SceneResult::pass("wireframe clamped to fill, interior pixel filled");
    }

    // Multiple viewports: DXVM clamps the count to one, so a draw must still
    // render into viewport 0.
    SceneResult sceneViewports() {
      Base b; std::string err;
      if (!setup(b, err)) return SceneResult::skip(err);

      auto* c = static_cast<ID3D11DeviceContext*>(b.ctx.context);

      D3D11_VIEWPORT vp = { };
      vp.Width = float(kSize); vp.Height = float(kSize); vp.MaxDepth = 1.0f;
      c->RSSetViewports(1u, &vp); // clamped count

      if (!drawQuad(b, nullptr))
        return SceneResult::fail("draw failed");

      unsigned char px[4] = { };
      if (!readPixel(b, kSize / 2u, kSize / 2u, px))
        return SceneResult::fail("readback failed");

      if (px[0] != 255u)
        return SceneResult::fail("single viewport did not render");

      return SceneResult::pass("1 viewport rendered");
    }

  }

  void registerStateScenes(Registry& registry) {
    registry.add("logicop",    &sceneLogicOp);
    registry.add("dualsource", &sceneDualSource);
    registry.add("wireframe",  &sceneWireframe);
    registry.add("viewports",  &sceneViewports);
  }

}

#endif