#pragma once

#include "framedata.h"

class ScriptedCamera {
public:
    ScriptedCamera();
    ~ScriptedCamera();
    
    // Initialize scripted camera at specified location
    void CreateCamera(Vector3 position, Vector3 rotation, float fov);
    
    // Destroy the scripted camera
    void DestroyCamera();
    
    // Set camera position and rotation
    void SetPosition(Vector3 position);
    void SetRotation(Vector3 rotation);
    void SetFOV(float fov);
    
    // Get current camera parameters
    Vector3 GetPosition() const;
    Vector3 GetRotation() const;
    CameraIntrinsics GetIntrinsics() const;
    CameraExtrinsics GetExtrinsics() const;
    
    // Randomize camera location to a different world position
    // Respects bounds to avoid spawning in voids
    void RandomizeLocation();
    
    // Toggle between scripted camera and default game camera
    void SetRendering(bool enabled);
    bool IsRendering() const;

    // Check if camera is active
    bool IsActive() const;
    
private:
    unsigned int camera_handle;
    bool is_active;
    bool is_rendering;
    
    // Helper: Get valid random world coordinates
    Vector3 GetRandomWorldPosition();
};
