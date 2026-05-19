#include "screencapturer.h"
#include "lodepng/lodepng.h"
#include <vector>

static const char* GTA_WINDOW_CLASS = "grcWindow";

ScreenCapturer::ScreenCapturer() : screen_width(0), screen_height(0) {
    InitializeScreenDimensions();
}

ScreenCapturer::~ScreenCapturer() {}

void ScreenCapturer::InitializeScreenDimensions() {
    // Best-effort at startup; the GTA V window may not exist yet.
    // CaptureScreenToHBITMAP refreshes these from the actual window each call.
    HWND hwnd = FindWindowA(GTA_WINDOW_CLASS, nullptr);
    if (hwnd) {
        RECT r;
        GetClientRect(hwnd, &r);
        screen_width  = r.right  - r.left;
        screen_height = r.bottom - r.top;
    } else {
        screen_width  = GetSystemMetrics(SM_CXSCREEN);
        screen_height = GetSystemMetrics(SM_CYSCREEN);
    }
}

int ScreenCapturer::GetScreenWidth()  const { return screen_width; }
int ScreenCapturer::GetScreenHeight() const { return screen_height; }

HBITMAP ScreenCapturer::CaptureScreenToHBITMAP() {
    HWND hwnd = FindWindowA(GTA_WINDOW_CLASS, nullptr);

    int w, h, srcX = 0, srcY = 0;

    if (hwnd) {
        // Client area size — excludes window chrome (irrelevant for borderless, but correct regardless)
        RECT client;
        GetClientRect(hwnd, &client);
        w = client.right  - client.left;
        h = client.bottom - client.top;

        // Map the client origin to screen coordinates so we BitBlt from the right place
        POINT origin = { 0, 0 };
        ClientToScreen(hwnd, &origin);
        srcX = origin.x;
        srcY = origin.y;
    } else {
        w = GetSystemMetrics(SM_CXSCREEN);
        h = GetSystemMetrics(SM_CYSCREEN);
    }

    screen_width  = w;
    screen_height = h;

    HDC     hScreenDC = GetDC(nullptr);
    HDC     hMemDC    = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap   = CreateCompatibleBitmap(hScreenDC, w, h);
    HBITMAP hOld      = static_cast<HBITMAP>(SelectObject(hMemDC, hBitmap));

    BitBlt(hMemDC, 0, 0, w, h, hScreenDC, srcX, srcY, SRCCOPY);

    SelectObject(hMemDC, hOld);
    DeleteDC(hMemDC);
    ReleaseDC(nullptr, hScreenDC);

    return hBitmap;
}

bool ScreenCapturer::CaptureScreenToPNG(const std::string& output_path) {
    HBITMAP hBitmap = CaptureScreenToHBITMAP();
    if (!hBitmap) return false;

    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    int w = bmp.bmWidth;
    int h = bmp.bmHeight;

    // Request 32-bit top-down BGRA from GDI (negative biHeight = top-down).
    BITMAPINFOHEADER bmih = {};
    bmih.biSize        = sizeof(BITMAPINFOHEADER);
    bmih.biWidth       = w;
    bmih.biHeight      = -h;
    bmih.biPlanes      = 1;
    bmih.biBitCount    = 32;
    bmih.biCompression = BI_RGB;

    HDC hScreenDC = GetDC(nullptr);
    HDC hMemDC    = CreateCompatibleDC(hScreenDC);
    SelectObject(hMemDC, hBitmap);

    std::vector<BYTE> pixels(w * h * 4);
    GetDIBits(hMemDC, hBitmap, 0, h, pixels.data(),
              reinterpret_cast<BITMAPINFO*>(&bmih), DIB_RGB_COLORS);

    DeleteDC(hMemDC);
    ReleaseDC(nullptr, hScreenDC);
    DeleteObject(hBitmap);

    // GDI gives BGRA with A=0; convert to RGBA with A=255 for lodepng.
    for (int i = 0; i < w * h; ++i) {
        BYTE b = pixels[i * 4 + 0];
        pixels[i * 4 + 0] = pixels[i * 4 + 2];  // R
        pixels[i * 4 + 2] = b;                   // B
        pixels[i * 4 + 3] = 255;
    }

    return lodepng_encode32_file(output_path.c_str(), pixels.data(), w, h) == 0;
}
