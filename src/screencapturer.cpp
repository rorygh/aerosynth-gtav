#include "screencapturer.h"
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

    int w           = bmp.bmWidth;
    int h           = bmp.bmHeight;
    int bit_count   = bmp.bmBitsPixel;
    int row_size    = ((w * bit_count + 31) / 32) * 4;

    BITMAPINFOHEADER bmih = {};
    bmih.biSize        = sizeof(BITMAPINFOHEADER);
    bmih.biWidth       = w;
    bmih.biHeight      = h;
    bmih.biPlanes      = 1;
    bmih.biBitCount    = static_cast<WORD>(bit_count);
    bmih.biCompression = BI_RGB;

    BITMAPFILEHEADER bmfh = {};
    bmfh.bfType    = 0x4D42;
    bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bmfh.bfSize    = bmfh.bfOffBits + row_size * h;

    HDC hScreenDC = GetDC(nullptr);
    HDC hMemDC    = CreateCompatibleDC(hScreenDC);
    SelectObject(hMemDC, hBitmap);

    std::vector<BYTE> pixels(row_size * h);
    GetDIBits(hMemDC, hBitmap, 0, h, pixels.data(),
              reinterpret_cast<BITMAPINFO*>(&bmih), DIB_RGB_COLORS);

    DeleteDC(hMemDC);
    ReleaseDC(nullptr, hScreenDC);
    DeleteObject(hBitmap);

    FILE* f = nullptr;
    if (fopen_s(&f, output_path.c_str(), "wb") != 0 || !f) return false;
    fwrite(&bmfh, sizeof(bmfh), 1, f);
    fwrite(&bmih, sizeof(bmih), 1, f);
    fwrite(pixels.data(), 1, pixels.size(), f);
    fclose(f);

    return true;
}
