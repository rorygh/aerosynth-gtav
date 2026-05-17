#include "depthcapturer.h"
#include "scripthookv_sdk/inc/main.h"
#include "scripthookv_sdk/inc/natives.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <cmath>
#include <cstdio>
#include <windows.h>

using Microsoft::WRL::ComPtr;

// ---- File-scope DirectX state (render thread only) ----
static ComPtr<ID3D11DeviceContext> g_ctx;
static ComPtr<ID3D11Texture2D>     g_depthTex;    // last suitable depth texture from OMSetRT
static ComPtr<ID3D11Texture2D>     g_stagingTex;
static void*                        g_origOMSetRT  = nullptr;
static void*                        g_origClearDSV = nullptr;

// ---- Static member definitions ----
const float DepthCapturer::MAX_DEPTH = 500.0f;

std::vector<float> DepthCapturer::s_rawDepths;
int                DepthCapturer::s_capWidth  = 0;
int                DepthCapturer::s_capHeight = 0;
std::mutex         DepthCapturer::s_mutex;
std::atomic<bool>  DepthCapturer::s_captureRequested{false};
std::atomic<bool>  DepthCapturer::s_depthReady{false};

bool         DepthCapturer::s_hooked       = false;
int          DepthCapturer::s_omSetRTCount = 0;
int          DepthCapturer::s_matchCount   = 0;
int          DepthCapturer::s_dsvCallCount = 0;
unsigned int DepthCapturer::s_lastFormat   = 0;
int          DepthCapturer::s_lastW        = 0;
int          DepthCapturer::s_lastH        = 0;
bool         DepthCapturer::s_lastMapOk    = false;
bool         DepthCapturer::s_timedOut     = false;

static void Log(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    FILE* f = nullptr;
    if (fopen_s(&f, "aerosynth_depth.log", "a") == 0 && f) {
        fprintf(f, "%s\n", buf);
        fclose(f);
    }
}

// ---- Callback registration ----

void DepthCapturer::RegisterCallback() { presentCallbackRegister(OnPresent); }
void DepthCapturer::UnregisterCallback() { presentCallbackUnregister(OnPresent); }

// ---- Vtable patching ----

static void PatchVTableSlot(void* instance, int slot, void* newFn, void** outOrig) {
    void** vtable = *reinterpret_cast<void***>(instance);
    DWORD old;
    VirtualProtect(&vtable[slot], sizeof(void*), PAGE_READWRITE, &old);
    *outOrig    = vtable[slot];
    vtable[slot] = newFn;
    VirtualProtect(&vtable[slot], sizeof(void*), old, &old);
}

// ---- OMSetRenderTargets hook (slot 33) ----
// Called whenever GTA V binds render targets; we track the main depth texture here.

using OMSetRTFn = void(__stdcall*)(void*, unsigned, void**, void*);

static void __stdcall OMSetRTHook(void* ctx, unsigned numViews, void** rtvs, void* dsv_ptr) {
    ++DepthCapturer::s_omSetRTCount;

    if (dsv_ptr) {
        auto* dsv = static_cast<ID3D11DepthStencilView*>(dsv_ptr);
        ComPtr<ID3D11Resource> res;
        dsv->GetResource(&res);
        ComPtr<ID3D11Texture2D> tex;
        if (SUCCEEDED(res.As(&tex))) {
            D3D11_TEXTURE2D_DESC desc;
            tex->GetDesc(&desc);
            // DXGI_FORMAT_R32G8X24_TYPELESS = 19
            if (desc.Format == DXGI_FORMAT_R32G8X24_TYPELESS &&
                desc.Width >= 600 && desc.Height >= 400)
            {
                g_depthTex = tex;
                DepthCapturer::s_lastFormat = static_cast<unsigned>(desc.Format);
                DepthCapturer::s_lastW      = static_cast<int>(desc.Width);
                DepthCapturer::s_lastH      = static_cast<int>(desc.Height);
                ++DepthCapturer::s_matchCount;
            }
        }
    }

    reinterpret_cast<OMSetRTFn>(g_origOMSetRT)(ctx, numViews, rtvs, dsv_ptr);
}

// ---- ClearDepthStencilView hook (slot 53) — diagnostic only ----

using ClearDSVFn = void(__stdcall*)(void*, void*, unsigned, float, unsigned char);

static void __stdcall ClearDSVHook(void* ctx, void* dsv, unsigned flags, float depth, unsigned char stencil) {
    ++DepthCapturer::s_dsvCallCount;
    reinterpret_cast<ClearDSVFn>(g_origClearDSV)(ctx, dsv, flags, depth, stencil);
}

// ---- Present callback (render thread, every frame) ----

void DepthCapturer::OnPresent(void* swapChain) {
    // One-time initialisation: get context, install vtable hooks
    if (!g_ctx) {
        auto* chain = static_cast<IDXGISwapChain*>(swapChain);
        ComPtr<ID3D11Device> dev;
        if (FAILED(chain->GetDevice(__uuidof(ID3D11Device),
                                    reinterpret_cast<void**>(dev.GetAddressOf())))) {
            Log("OnPresent: GetDevice failed");
            return;
        }
        dev->GetImmediateContext(&g_ctx);
        PatchVTableSlot(g_ctx.Get(), 33, reinterpret_cast<void*>(&OMSetRTHook),  &g_origOMSetRT);
        PatchVTableSlot(g_ctx.Get(), 53, reinterpret_cast<void*>(&ClearDSVHook), &g_origClearDSV);
        s_hooked = true;
        Log("OnPresent: hooks installed (OMSetRT=33, ClearDSV=53)");
    }

    if (!s_captureRequested.load(std::memory_order_relaxed)) return;

    if (!g_depthTex) {
        // Log once so the user can see we're waiting
        static int noTexLog = 0;
        if (noTexLog++ < 5) Log("OnPresent: capture pending, no depth texture yet");
        return;
    }

    D3D11_TEXTURE2D_DESC desc;
    g_depthTex->GetDesc(&desc);

    // Create or recreate staging texture when dimensions change
    if (g_stagingTex) {
        D3D11_TEXTURE2D_DESC sd;
        g_stagingTex->GetDesc(&sd);
        if (sd.Width != desc.Width || sd.Height != desc.Height) g_stagingTex.Reset();
    }
    if (!g_stagingTex) {
        ComPtr<ID3D11Device> dev;
        g_ctx->GetDevice(&dev);
        D3D11_TEXTURE2D_DESC sd = desc;
        sd.BindFlags      = 0;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        sd.Usage          = D3D11_USAGE_STAGING;
        sd.MiscFlags      = 0;
        HRESULT hr = dev->CreateTexture2D(&sd, nullptr, &g_stagingTex);
        if (FAILED(hr)) {
            Log("OnPresent: CreateTexture2D failed hr=0x%08X", hr);
            return;
        }
        Log("OnPresent: staging texture created %ux%u", desc.Width, desc.Height);
    }

    g_ctx->CopyResource(g_stagingTex.Get(), g_depthTex.Get());

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = g_ctx->Map(g_stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    s_lastMapOk = SUCCEEDED(hr);
    if (s_lastMapOk) {
        Log("OnPresent: depth captured %ux%u rowPitch=%u", desc.Width, desc.Height, mapped.RowPitch);
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_capWidth  = static_cast<int>(desc.Width);
            s_capHeight = static_cast<int>(desc.Height);
            s_rawDepths.resize(s_capWidth * s_capHeight);
            // Layout: 8 bytes/pixel — [float32 depth][uint8 stencil][3 bytes pad]
            for (int y = 0; y < s_capHeight; ++y) {
                for (int x = 0; x < s_capWidth; ++x) {
                    const float* px = reinterpret_cast<const float*>(
                        static_cast<const char*>(mapped.pData) +
                        mapped.RowPitch * y + x * 8);
                    s_rawDepths[y * s_capWidth + x] = *px;
                }
            }
        }
        g_ctx->Unmap(g_stagingTex.Get(), 0);
        s_captureRequested.store(false, std::memory_order_relaxed);
        s_depthReady.store(true, std::memory_order_release);
    } else {
        Log("OnPresent: Map FAILED hr=0x%08X", hr);
    }
}

// ---- Script-thread API ----

bool DepthCapturer::SaveDepth(const std::string& path, float near_clip, float far_clip) {
    s_timedOut = false;
    if (!s_hooked) return false;

    s_depthReady.store(false, std::memory_order_relaxed);
    s_captureRequested.store(true, std::memory_order_release);

    int ticks = 0;
    while (!s_depthReady.load(std::memory_order_acquire) && ticks < 300) {
        WAIT(0);
        ++ticks;
    }

    if (!s_depthReady.load(std::memory_order_acquire)) {
        s_captureRequested.store(false, std::memory_order_relaxed);
        s_timedOut = true;
        Log("SaveDepth: timed out after %d ticks", ticks);
        return false;
    }

    std::vector<float> raw;
    int w, h;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        raw = s_rawDepths;
        w   = s_capWidth;
        h   = s_capHeight;
    }
    if (raw.empty()) return false;

    // Linearise NDC depth to world-space metres.
    // Standard DX projection: ndc = Q - Q*nc/z  =>  z = Q*nc / (Q - ndc)
    const float Q = far_clip / (far_clip - near_clip);
    std::vector<float> linear(w * h);
    for (int i = 0; i < w * h; ++i) {
        float ndc = raw[i];
        if (ndc < 0.0f) ndc = 0.0f;
        if (ndc > 1.0f) ndc = 1.0f;
        linear[i] = Q * near_clip / (Q - ndc);
    }

    return WriteBMP16(path, linear, w, h);
}

bool DepthCapturer::WriteBMP16(const std::string& path,
                               const std::vector<float>& linear_depths,
                               int width, int height)
{
    const int stride = width * 2;

    BITMAPFILEHEADER fh = {};
    fh.bfType    = 0x4D42;
    fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fh.bfSize    = fh.bfOffBits + stride * height;

    BITMAPINFOHEADER ih = {};
    ih.biSize        = sizeof(BITMAPINFOHEADER);
    ih.biWidth       = width;
    ih.biHeight      = -height;
    ih.biPlanes      = 1;
    ih.biBitCount    = 16;
    ih.biCompression = BI_RGB;
    ih.biSizeImage   = stride * height;

    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return false;

    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);

    std::vector<uint8_t> row(stride);
    for (int v = 0; v < height; ++v) {
        for (int u = 0; u < width; ++u) {
            float d = linear_depths[v * width + u];
            if (d < 0.0f)      d = 0.0f;
            if (d > MAX_DEPTH) d = MAX_DEPTH;
            uint16_t val = static_cast<uint16_t>(d / MAX_DEPTH * 65535.0f);
            row[u * 2 + 0] = val & 0xFF;
            row[u * 2 + 1] = (val >> 8) & 0xFF;
        }
        fwrite(row.data(), 1, stride, f);
    }

    fclose(f);
    return true;
}
