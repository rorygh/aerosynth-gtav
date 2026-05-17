#include "screencapturer.h"
#include <cstring>
#include <vector>

// Simple PNG encoding (stb_image_write style approach)
// This is a minimal PNG writer - production code would use libpng or lodepng
// For now, we'll write uncompressed BMP and note that Phase 1 should be PNG via external lib

ScreenCapturer::ScreenCapturer() : screen_width(0), screen_height(0) {
    InitializeScreenDimensions();
}

ScreenCapturer::~ScreenCapturer() {
}

void ScreenCapturer::InitializeScreenDimensions() {
    screen_width = GetSystemMetrics(SM_CXSCREEN);
    screen_height = GetSystemMetrics(SM_CYSCREEN);
}

int ScreenCapturer::GetScreenWidth() const {
    return screen_width;
}

int ScreenCapturer::GetScreenHeight() const {
    return screen_height;
}

HBITMAP ScreenCapturer::CaptureScreenToHBITMAP() {
    HDC hScreenDC = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, screen_width, screen_height);
    
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
    
    BitBlt(hMemDC, 0, 0, screen_width, screen_height, hScreenDC, 0, 0, SRCCOPY);
    
    SelectObject(hMemDC, hOldBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);
    
    return hBitmap;
}

bool ScreenCapturer::CaptureScreenToPNG(const std::string& output_path) {
    HBITMAP hBitmap = CaptureScreenToHBITMAP();
    if (!hBitmap) {
        return false;
    }
    
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    
    // Get device context for screen
    HDC hScreenDC = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    SelectObject(hMemDC, hBitmap);
    
    // Prepare BMP file header
    int width = bmp.bmWidth;
    int height = bmp.bmHeight;
    int bit_count = bmp.bmBitsPixel;
    int bytes_per_pixel = bit_count / 8;
    int row_size = ((width * bit_count + 31) / 32) * 4;
    
    // For MVP, write as BMP (simpler than PNG encoding from scratch)
    // TODO: Add lodepng library to external/ and encode as PNG
    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;
    
    bmih.biSize = sizeof(BITMAPINFOHEADER);
    bmih.biWidth = width;
    bmih.biHeight = height;
    bmih.biPlanes = 1;
    bmih.biBitCount = bit_count;
    bmih.biCompression = BI_RGB;
    bmih.biSizeImage = 0;
    bmih.biXPelsPerMeter = 0;
    bmih.biYPelsPerMeter = 0;
    bmih.biClrUsed = 0;
    bmih.biClrImportant = 0;
    
    bmfh.bfType = 0x4D42; // "BM"
    bmfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + row_size * height;
    bmfh.bfReserved1 = 0;
    bmfh.bfReserved2 = 0;
    bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    
    // Read bitmap data
    std::vector<BYTE> pixel_data(row_size * height);
    GetDIBits(hMemDC, hBitmap, 0, height, pixel_data.data(), (BITMAPINFO*)&bmih, DIB_RGB_COLORS);
    
    // Write to file
    FILE* file = nullptr;
    errno_t err = fopen_s(&file, output_path.c_str(), "wb");
    if (err != 0 || !file) {
        DeleteDC(hMemDC);
        ReleaseDC(NULL, hScreenDC);
        DeleteObject(hBitmap);
        return false;
    }
    
    fwrite(&bmfh, sizeof(BITMAPFILEHEADER), 1, file);
    fwrite(&bmih, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(pixel_data.data(), 1, pixel_data.size(), file);
    
    fclose(file);
    
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);
    DeleteObject(hBitmap);
    
    return true;
}
