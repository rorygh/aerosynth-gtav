#pragma once

#include <string>
#include <ctime>
#include "scripthookv_sdk/inc/types.h"

struct CameraIntrinsics {
    float fov;              // Field of view in degrees
    float near_clip;        // Near clipping plane
    float far_clip;         // Far clipping plane
    float aspect_ratio;     // Width / Height
};

struct CameraExtrinsics {
    Vector3 position;       // World-space camera position
    Vector3 rotation;       // Euler angles (x, y, z)
};

struct FrameResolution {
    int width;
    int height;
};

struct FrameData {
    // Frame metadata
    std::string frame_id;           // Unique identifier (timestamp_frameN)
    long long timestamp;            // Unix timestamp (milliseconds)
    int frame_number;               // Frame index in sequence
    FrameResolution resolution;     // Screen resolution
    
    // Camera parameters
    CameraIntrinsics intrinsics;
    CameraExtrinsics extrinsics;
    
    // File paths (relative to capture session directory)
    std::string rgb_filename;       // e.g., "frame_000000.png"
    std::string depth_filename;     // e.g., "frame_000000_depth.png" (optional)
    std::string segmentation_filename;  // e.g., "frame_000000_segmentation.png" (optional)
};
