# AeroSynth GTA V — Synthetic Aerial Data Mod

A ScriptHookV ASI mod for GTA V that captures synthetic aerial data for a drone ML pipeline.

## Project Goal

Capture the following from a scripted aerial camera:

- RGB image/screenshot
- Camera intrinsics and extrinsics
- Depth mask
- Segmentation mask *(Phase 3)*
- Stereo images *(future)*

## Build

```powershell
# Launch the Visual Studio dev shell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64

cmake -B build -G "Ninja"
cmake --build build --config Release
cmake --install build --config Release
```

`cmake --install` copies the `.asi` to the GTA V directory configured in `CMakePresets.json` or via `-DCMAKE_INSTALL_PREFIX`.

The Ninja generator enables `compile_commands.json` for clangd IntelliSense.

## Key Files

| File | Purpose |
|------|---------|
| `src/script.cpp` | Main loop, hotkeys, `CaptureFrame`, `RandomizeDronePosition` |
| `src/camera.h/cpp` | `ScriptedCamera` — wraps CAM natives, exposes intrinsics/extrinsics |
| `src/screencapturer.h/cpp` | Windows GDI screen capture, saves BMP |
| `src/depthcapturer.h/cpp` | DirectX depth buffer readback, saves 24-bit grayscale BMP |
| `src/fileexporter.h/cpp` | Session directory init, per-frame JSON metadata |
| `src/framedata.h` | `FrameData`, `CameraIntrinsics`, `CameraExtrinsics` structs |

## Hotkeys

| Key | Action |
|-----|--------|
| F6 | Capture single frame |
| F7 | Toggle continuous capture (~20 fps) |
| F8 | Teleport to random drone position |
| F9 | Toggle aerial scripted camera / normal game camera |

## Output Structure

```
captures/
  YYYY-MM-DD_HH-MM-SS/
    frame_000000.bmp           # left RGB
    frame_000000_right.bmp     # right RGB (stereo, 1 m baseline)
    frame_000000_depth.bmp     # left depth only
    frame_000000_seg.bmp       # left segmentation only
    frame_000000.json          # metadata for all of the above
    ...
```

Depth is a 24-bit grayscale BMP (R=G=B), linear view-space Z: `0 = 0 m`, `255 = 500 m`.
Decode: `depth_metres = pixel_value / 255.0 * 500.0`. Anything beyond 500 m is clamped to 255.

## GTA V Technical Notes

**Coordinate system:** +X = East, +Y = North, +Z = Up.  
**Euler rotation order:** ZXY (pitch = rotX, yaw = rotZ, roll ignored).

**Asset streaming:** GTA V uses a player-centric LOD system. Before querying terrain, call `REQUEST_COLLISION_AT_COORD`, `REQUEST_ADDITIONAL_COLLISION_AT_COORD`, and `LOAD_SCENE`, then `WAIT(2500)`.

**Ground height:** Use `GET_GROUND_Z_FOR_3D_COORD` probing from Z=300. Reject results above 250 m — the function can echo the probe altitude when collision hasn't loaded. Check `WATER::GET_WATER_HEIGHT` and use the higher of ground/water.

**Debug text:** Use `UI::_SET_TEXT_ENTRY("CELL_EMAIL_BCON")` for multi-line HUD text. The default `"STRING"` label silently truncates at ~99 characters. `_ADD_TEXT_COMPONENT_STRING` also truncates at 99 characters **per call** even with `CELL_EMAIL_BCON` — split long strings into ≤99-character chunks and call it once per chunk.

**Raycasting:** GTA V's concurrent raycast queue is very small (~10–20 handles). Fire each ray and immediately read its result in the same loop iteration (synchronous). Bulk-firing all rays before collecting results causes all but the first ~20 to return no-hit (all-white depth mask).

**Player state:** Invincibility and invisibility reset when the player respawns (new ped handle). `MaintainPlayerState()` re-applies them every tick.

## References

- `references/DeepGTAV` — Similar ScriptHookV data-capture mod for autonomous driving. Poor code quality; useful for native API patterns (especially LiDAR raycasting).
- `references/samples` — Official ScriptHookV SDK samples. Good quality; useful for native API usage.

## Dependencies

- **ScriptHookV SDK** — `external/scripthookv_sdk/`
- **RapidJSON** — header-only JSON, `external/rapidjson/`
- **DirectX 11** — `d3d11.lib` / `dxgi.lib` (Windows SDK, no external download needed)
- No external image libraries; BMP is written directly. PNG migration deferred (may add vcpkg/Conan for lodepng later).

## Implementation Status

### Phase 1 — RGB + Camera Tracking ✅
- Frame metadata structs (`framedata.h`)
- RGB screen capture via Windows GDI
- Scripted camera system (CAM natives), F9 toggle aerial/normal
- Hotkey triggers (F6 single, F7 continuous, F8 random drone position)
- File exporter: BMP + JSON per frame

### Phase 2 — Depth Mask ✅
Game resolution is set to **1280×720**; depth mask matches this resolution.

Approach: read the actual DirectX depth buffer (pixel-accurate, no raycasts).

Pipeline:
1. `presentCallbackRegister` (DLL attach) → render-thread callback fires each frame
2. First present: get `ID3D11Device`/`ID3D11DeviceContext` from `IDXGISwapChain`; patch vtable slots 33 (`OMSetRenderTargets`) and 53 (`ClearDepthStencilView`)
3. `OMSetRenderTargets` hook tracks the currently-bound DSV; when GTA V **unbinds** the main depth texture (`DXGI_FORMAT_R32G8X24_TYPELESS`, width ≥ 600), the scene pass is complete — `CopyResource` to a staging texture immediately
4. `OnPresent`: `Map` the staging texture → extract 32-bit float NDC depths → signal script thread
5. Script thread requests a capture, yields via `WAIT(0)` until render thread delivers, then linearises + writes BMP

**GTA V uses reversed-Z** (near = 1.0, far = 0.0). Linearisation flips NDC before applying the standard formula: `ndc_fwd = 1 − ndc_raw`, then `z = Q × near / (Q − ndc_fwd)` where `Q = far / (far − near)`.

**`ClearDepthStencilView` (slot 53) cannot be used for capture timing** — GTA V caches the function pointer directly and bypasses the vtable, so the hook never fires for GTA V's own clears. The slot-33 unbind approach works around this.

Output: 24-bit grayscale BMP (R=G=B), linear view-space Z, 0–500 m.

### Phase 3 — Segmentation Masks ✅
Primary targets: foliage/trees (stencil 3) and natural ground (stencil 4).

**Stencil buffer class mapping** — empirically determined from GTA V captures.
The 8-bit stencil channel of the `DXGI_FORMAT_R32G8X24_TYPELESS` depth buffer encodes semantic classes:

| Stencil | Class | Palette colour |
|---------|-------|---------------|
| 0 | Inanimate objects + artificial surfaces (roads, buildings, props) | Dark grey |
| 1 | Persons / pedestrians | Red |
| 2 | Vehicles | Blue |
| 3 | Foliage / trees | Green |
| 4 | Natural ground (terrain surface, grass) | Brown |
| 7 | Sky | Sky blue |
| 5, 6, 8–15 | Unassigned (not yet observed) | Various |

The stencil is captured for free alongside the depth buffer (same `R32G8X24` staging texture, byte offset +4 per pixel). `DepthCapturer::SaveSegmentation` writes a false-colour 24-bit BMP using this palette and logs the per-value pixel distribution to `aerosynth_depth.log`.

### Phase 4 — Polish
- Performance profiling
- ~~Stereo image capture~~ ✅ (see below)
- PNG migration

### Stereo Capture ✅
Baseline: **1 m**, horizontal, along the camera's local right axis.

Right-camera position offset from left: `right = left_pos + baseline × (cos rotZ, sin rotZ, 0)` — the camera's local X axis in world space for ZXY Euler with roll=0. The baseline lies flat in the world plane regardless of camera pitch.

Sequence per frame:
1. Left RGB + depth + seg (existing)
2. `SetPosition(right_pos)` → `WAIT(0)` → GDI-grab right RGB
3. `SetPosition(left_pos)` restore (no extra WAIT)

Only RGB is captured for the right eye. Both camera positions are written to the JSON under `camera` (left) and `camera_right` (right). Intrinsics are shared.
