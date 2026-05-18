#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <mutex>

class DepthCapturer {
public:
    static const float MAX_DEPTH;   // metres — for 16-bit BMP encoding

    // Must be called from DllMain DLL_PROCESS_ATTACH
    static void RegisterCallback();
    // Must be called from DllMain DLL_PROCESS_DETACH
    static void UnregisterCallback();

    // Request one depth+stencil frame. Blocks (via WAIT) until the render thread delivers.
    // Returns false if capture failed or timed out.
    bool SaveDepth(const std::string& path, float near_clip, float far_clip);

    // Write a false-colour segmentation BMP from the stencil captured by the last SaveDepth call.
    // Must be called after a successful SaveDepth; does not block.
    bool SaveSegmentation(const std::string& path);

    // Diagnostic state readable from the script thread for HUD display
    static bool         s_hooked;
    static int          s_omSetRTCount;   // total OMSetRenderTargets calls seen
    static int          s_matchCount;     // calls where a suitable depth texture was saved
    static int          s_dsvCallCount;   // total ClearDepthStencilView calls seen (expect 0)
    static unsigned int s_lastFormat;     // DXGI_FORMAT of last matched depth texture
    static int          s_lastW;
    static int          s_lastH;
    static bool         s_lastMapOk;      // whether the most recent Map() in OnPresent succeeded
    static bool         s_timedOut;       // set when SaveDepth gives up waiting
    static std::atomic<bool>  s_captureRequested;

private:
    static void OnPresent(void* swapChain);

    bool WriteBMPGray(const std::string& path,
                      const std::vector<float>& linear_depths,
                      int width, int height);

    bool WriteSegBMP(const std::string& path,
                     const std::vector<uint8_t>& stencil,
                     int width, int height);

    static std::vector<float>   s_rawDepths;
    static std::vector<uint8_t> s_rawStencil;
    static int                s_capWidth;
    static int                s_capHeight;
    static std::mutex         s_mutex;
    static std::atomic<bool>  s_depthReady;
};
