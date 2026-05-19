#include "camera.h"
#include "scripthookv_sdk/inc/main.h"
#include "scripthookv_sdk/inc/natives.h"
#include <cmath>
#include <cstdlib>
#include <ctime>

ScriptedCamera::ScriptedCamera() : camera_handle(0), is_active(false), is_rendering(false) {
}

ScriptedCamera::~ScriptedCamera() {
    if (is_active) {
        DestroyCamera();
    }
}

void ScriptedCamera::CreateCamera(Vector3 position, Vector3 rotation, float fov) {
    // Destroy any existing camera
    if (is_active) {
        DestroyCamera();
    }
    
    // Create new camera
    camera_handle = CAM::CREATE_CAM("DEFAULT_SCRIPTED_CAMERA", TRUE);
    
    // Set initial parameters
    CAM::SET_CAM_COORD(camera_handle, position.x, position.y, position.z);
    CAM::SET_CAM_ROT(camera_handle, rotation.x, rotation.y, rotation.z, 2); // 2 = Euler angles
    CAM::SET_CAM_FOV(camera_handle, fov);
    
    // Activate camera and tell GTA to render from it
    CAM::SET_CAM_ACTIVE(camera_handle, TRUE);
    CAM::RENDER_SCRIPT_CAMS(TRUE, FALSE, 3000, TRUE, FALSE);

    is_active = true;
    is_rendering = true;
}

void ScriptedCamera::DestroyCamera() {
    if (is_active && camera_handle) {
        CAM::RENDER_SCRIPT_CAMS(FALSE, FALSE, 3000, TRUE, FALSE);
        CAM::SET_CAM_ACTIVE(camera_handle, FALSE);
        CAM::DESTROY_CAM(camera_handle, FALSE);
        is_active = false;
        is_rendering = false;
    }
}

void ScriptedCamera::SetRendering(bool enabled) {
    if (!is_active) return;
    CAM::SET_CAM_ACTIVE(camera_handle, enabled ? TRUE : FALSE);
    CAM::RENDER_SCRIPT_CAMS(enabled ? TRUE : FALSE, TRUE, 3000, TRUE, FALSE);
    is_rendering = enabled;
}

bool ScriptedCamera::IsRendering() const {
    return is_rendering;
}

void ScriptedCamera::SetPosition(Vector3 position) {
    if (is_active) {
        CAM::SET_CAM_COORD(camera_handle, position.x, position.y, position.z);
    }
}

void ScriptedCamera::SetRotation(Vector3 rotation) {
    if (is_active) {
        CAM::SET_CAM_ROT(camera_handle, rotation.x, rotation.y, rotation.z, 2);
    }
}

void ScriptedCamera::SetFOV(float fov) {
    if (is_active) {
        CAM::SET_CAM_FOV(camera_handle, fov);
    }
}

Vector3 ScriptedCamera::GetPosition() const {
    Vector3 pos;
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    
    if (is_active) {
        Vector3 cam_pos = CAM::GET_CAM_COORD(camera_handle);
        pos.x = cam_pos.x;
        pos.y = cam_pos.y;
        pos.z = cam_pos.z;
    }
    return pos;
}

Vector3 ScriptedCamera::GetRotation() const {
    Vector3 rot;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    
    if (is_active) {
        Vector3 cam_rot = CAM::GET_CAM_ROT(camera_handle, 2);
        rot.x = cam_rot.x;
        rot.y = cam_rot.y;
        rot.z = cam_rot.z;
    }
    return rot;
}

CameraIntrinsics ScriptedCamera::GetIntrinsics() const {
    CameraIntrinsics intrinsics = { 50.0f, 0.1f, 5000.0f, 1.777778f };
    
    if (is_active) {
        intrinsics.fov = CAM::GET_CAM_FOV(camera_handle);
        intrinsics.near_clip = CAM::GET_CAM_NEAR_CLIP(camera_handle);
        intrinsics.far_clip = CAM::GET_CAM_FAR_CLIP(camera_handle);
        
        // Get screen aspect ratio from game
        intrinsics.aspect_ratio = GRAPHICS::_GET_SCREEN_ASPECT_RATIO(FALSE);
    }
    
    return intrinsics;
}

CameraExtrinsics ScriptedCamera::GetExtrinsics() const {
    CameraExtrinsics extrinsics;
    extrinsics.position = GetPosition();
    extrinsics.rotation = GetRotation();
    return extrinsics;
}

Vector3 ScriptedCamera::GetRandomWorldPosition() {
    float x = -300.0f + static_cast<float>(rand()) / (RAND_MAX / 700.0f);
    float y = -2000.0f + static_cast<float>(rand()) / (RAND_MAX / 3500.0f);
    float z = 50.0f + static_cast<float>(rand()) / (RAND_MAX / 150.0f);
    
    Vector3 pos;
    pos.x = x;
    pos.y = y;
    pos.z = z;
    return pos;
}

void ScriptedCamera::RandomizeLocation() {
    if (is_active) {
        Vector3 new_pos = GetRandomWorldPosition();
        Vector3 new_rot;
        // Pitch: -90 (nadir) to -20 (shallow oblique), random yaw, no roll
        new_rot.x = -20.0f - static_cast<float>(rand() % 71); // -20 to -90
        new_rot.y = 0.0f;
        new_rot.z = static_cast<float>(rand() % 360);

        SetPosition(new_pos);
        SetRotation(new_rot);
    }
}

bool ScriptedCamera::IsActive() const {
    return is_active;
}

Vector3 ScriptedCamera::GetStereoRightPosition(float baseline_m) const {
    Vector3 pos = GetPosition();
    Vector3 rot = GetRotation();
    // ZXY Euler, roll=0: camera local-X in world space = (cos rotZ, sin rotZ, 0).
    // Baseline stays horizontal — no z component.
    float rad = rot.z * (3.14159265358979f / 180.0f);
    pos.x += baseline_m * cosf(rad);
    pos.y += baseline_m * sinf(rad);
    return pos;
}
