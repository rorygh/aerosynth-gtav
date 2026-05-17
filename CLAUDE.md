# AeroSynth GTA V — Synthetic Aerial Data Mod

A ScriptHookV ASI mod for GTA V that captures synthetic aerial data for a drone ML pipeline.

## Project Goal

Capture the following from a scripted aerial camera:

- RGB image/screenshot
- Depth mask
- Segmentation mask *(Phase 3)*
- Camera intrinsics and extrinsics
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
| `src/depthcapturer.h/cpp` | Per-ray synchronous raycast depth, saves 16-bit BMP |
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
    frame_000000.bmp
    frame_000000_depth.bmp
    frame_000000.json
    ...
```

Depth is 16-bit BMP, linear: `0 = 0 m`, `65535 = 500 m`.

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

### Phase 2 — Depth Mask 🔄
Game resolution is set to **1280×720**; depth mask matches this resolution.

Approach: read the actual DirectX depth buffer (pixel-accurate, no raycasts).

Pipeline:
1. `presentCallbackRegister` (DLL attach) → render-thread callback fires each frame
2. First present: get `ID3D11Device`/`ID3D11DeviceContext` from `IDXGISwapChain`
3. Patch vtable slot 53 (`ClearDepthStencilView`) with a hook
4. Hook fires when GTA V clears the main depth buffer (`DXGI_FORMAT_R32G8X24_TYPELESS`, width ≥ 600) — right after the scene is drawn
5. `CopyResource` to a CPU-readable staging texture; `Map` → extract 32-bit float NDC depths
6. Script thread requests a capture, yields via `WAIT(0)` until render thread delivers, then linearises + writes 16-bit BMP

Depth linearisation: `z = Q * near / (Q − ndc)` where `Q = far / (far − near)` (standard DX projection).
Output: 16-bit BMP, linear 0–500 m.

### Phase 3 — Segmentation Masks
- Entity detection for natural features (trees, buildings, road, water)
- Per-pixel class label mask

### Phase 4 — Polish
- Performance profiling
- Stereo image capture
- PNG migration
