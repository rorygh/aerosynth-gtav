#include "fileexporter.h"
#include <windows.h>
#include <direct.h>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <fstream>

FileExporter::FileExporter() {
}

FileExporter::~FileExporter() {
}

std::string FileExporter::GetTimestampString() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    
    std::ostringstream oss;
    oss << std::put_time(&timeinfo, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

bool FileExporter::CreateDirectoryIfNeeded(const std::string& dir) {
    if (_mkdir(dir.c_str()) == 0) {
        return true; // Created successfully
    }
    
    // Check if already exists
    DWORD attrs = GetFileAttributesA(dir.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY));
}

std::string FileExporter::InitializeSession() {
    std::string timestamp = GetTimestampString();
    session_directory = "captures/" + timestamp;
    
    // Create base directory
    CreateDirectoryIfNeeded("captures");
    
    // Create session directory
    if (!CreateDirectoryIfNeeded(session_directory)) {
        return "";
    }

    return session_directory;
}

std::string FileExporter::GetSessionDirectory() const {
    return session_directory;
}

std::string FileExporter::GetFrameFilename(int frame_number, const std::string& suffix) {
    std::ostringstream oss;
    oss << "frame_" << std::setfill('0') << std::setw(6) << frame_number;
    if (!suffix.empty()) {
        oss << "_" << suffix;
    }
    oss << ".png";
    return oss.str();
}

bool FileExporter::WriteFrameMetadata(const FrameData& frame_data, const std::string& json_filepath) {
    // Simple JSON writing without external library
    // Format camera matrix as 4x4 for intrinsics/extrinsics
    
    std::ofstream file(json_filepath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "{\n";
    file << "  \"timestamp\": " << frame_data.timestamp << ",\n";
    file << "  \"frame_number\": " << frame_data.frame_number << ",\n";
    file << "  \"resolution\": {\n";
    file << "    \"width\": " << frame_data.resolution.width << ",\n";
    file << "    \"height\": " << frame_data.resolution.height << "\n";
    file << "  },\n";
    
    file << "  \"camera\": {\n";
    file << "    \"position\": {\n";
    file << "      \"x\": " << frame_data.extrinsics.position.x << ",\n";
    file << "      \"y\": " << frame_data.extrinsics.position.y << ",\n";
    file << "      \"z\": " << frame_data.extrinsics.position.z << "\n";
    file << "    },\n";
    file << "    \"rotation\": {\n";
    file << "      \"x\": " << frame_data.extrinsics.rotation.x << ",\n";
    file << "      \"y\": " << frame_data.extrinsics.rotation.y << ",\n";
    file << "      \"z\": " << frame_data.extrinsics.rotation.z << "\n";
    file << "    },\n";
    file << "    \"intrinsics\": {\n";
    file << "      \"fov\": " << frame_data.intrinsics.fov << ",\n";
    file << "      \"near_clip\": " << frame_data.intrinsics.near_clip << ",\n";
    file << "      \"far_clip\": " << frame_data.intrinsics.far_clip << ",\n";
    file << "      \"aspect_ratio\": " << frame_data.intrinsics.aspect_ratio << "\n";
    file << "    }\n";
    file << "  },\n";

    if (!frame_data.right_rgb_filename.empty()) {
        file << "  \"camera_right\": {\n";
        file << "    \"position\": {\n";
        file << "      \"x\": " << frame_data.right_extrinsics.position.x << ",\n";
        file << "      \"y\": " << frame_data.right_extrinsics.position.y << ",\n";
        file << "      \"z\": " << frame_data.right_extrinsics.position.z << "\n";
        file << "    },\n";
        file << "    \"rotation\": {\n";
        file << "      \"x\": " << frame_data.right_extrinsics.rotation.x << ",\n";
        file << "      \"y\": " << frame_data.right_extrinsics.rotation.y << ",\n";
        file << "      \"z\": " << frame_data.right_extrinsics.rotation.z << "\n";
        file << "    }\n";
        file << "  },\n";
    }
    
    file << "  \"files\": {\n";
    file << "    \"rgb\": \"" << frame_data.rgb_filename << "\"";
    if (!frame_data.depth_filename.empty()) {
        file << ",\n    \"depth\": \"" << frame_data.depth_filename << "\"";
    }
    if (!frame_data.segmentation_filename.empty()) {
        file << ",\n    \"segmentation\": \"" << frame_data.segmentation_filename << "\"";
    }
    if (!frame_data.right_rgb_filename.empty()) {
        file << ",\n    \"right\": \"" << frame_data.right_rgb_filename << "\"";
    }
    file << "\n";
    file << "  }\n";
    file << "}\n";
    
    file.close();
    return true;
}

bool FileExporter::ExportFrame(const FrameData& frame_data, const std::string& image_filepath) {
    if (session_directory.empty()) {
        return false;
    }

    // Verify image file exists
    std::ifstream img_file(image_filepath);
    if (!img_file.good()) {
        return false;
    }
    img_file.close();

    // Derive JSON filename from frame_number so it always matches the image filename
    std::string frame_filename = GetFrameFilename(frame_data.frame_number);
    std::string base_name = frame_filename.substr(0, frame_filename.find_last_of('.'));
    std::string full_json_path = session_directory + "/" + base_name + ".json";

    return WriteFrameMetadata(frame_data, full_json_path);
}
