#pragma once

#include <string>
#include <windows.h>

class ScreenCapturer {
public:
    ScreenCapturer();
    ~ScreenCapturer();
    
    // Capture current screen and save to PNG file
    // Returns true on success
    bool CaptureScreenToPNG(const std::string& output_path);
    
    // Get screen dimensions
    int GetScreenWidth() const;
    int GetScreenHeight() const;
    
private:
    int screen_width;
    int screen_height;
    
    // Helper: Initialize screen dimensions
    void InitializeScreenDimensions();
    
    // Helper: Create device contexts for capture
    HBITMAP CaptureScreenToHBITMAP();
};
