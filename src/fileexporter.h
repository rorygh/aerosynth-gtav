#pragma once

#include "framedata.h"
#include <string>

class FileExporter {
public:
    FileExporter();
    ~FileExporter();
    
    // Initialize exporter: creates capture session directory
    // Returns session directory path on success
    std::string InitializeSession();
    
    // Export frame data: saves image and metadata
    // Returns true on success
    bool ExportFrame(const FrameData& frame_data, const std::string& image_filepath);
    
    // Get current session directory
    std::string GetSessionDirectory() const;
    
    // Get formatted filename for frame
    std::string GetFrameFilename(int frame_number, const std::string& suffix = "");
    
private:
    std::string session_directory;
    int frame_count;
    
    // Helper: Create directory if it doesn't exist
    bool CreateDirectoryIfNeeded(const std::string& dir);
    
    // Helper: Get current timestamp as YYYY-MM-DD_HH-MM-SS
    std::string GetTimestampString();
    
    // Helper: Write frame metadata to JSON file
    bool WriteFrameMetadata(const FrameData& frame_data, const std::string& json_filepath);
};
