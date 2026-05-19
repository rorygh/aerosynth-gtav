/*
	THIS FILE IS A PART OF GTA V SCRIPT HOOK SDK
				http://dev-c.com			
			(C) Alexander Blade 2015
*/

/*
	AEROSYNTH Data Capture Script
	Hotkeys:
	- F6 (VK 0x75): Capture single frame
	- F7 (VK 0x76): Toggle continuous capture on/off
	- F8 (VK 0x77): Teleport to random drone position
	- F9 (VK 0x78): Toggle aerial / normal camera
*/

#include "script.h"
#include "keyboard.h"
#include "screencapturer.h"
#include "camera.h"
#include "fileexporter.h"
#include "depthcapturer.h"
#include "scripthookv_sdk/inc/natives.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>

// Global objects
ScreenCapturer g_screen_capturer;
ScriptedCamera g_camera;
FileExporter   g_file_exporter;
DepthCapturer  g_depth_capturer;

// State variables
bool g_capture_enabled = false;
bool g_continuous_capture = false;
std::string g_session_directory = "";
int frame_count = 0;
float g_cam_altitude_offset = 35.0f; // metres above player; randomised by F8

void SetupPlayer()
{
	Player player_id = PLAYER::PLAYER_ID();
	Ped player_ped = PLAYER::PLAYER_PED_ID();

	PLAYER::SET_PLAYER_INVINCIBLE(player_id, TRUE);
	ENTITY::SET_ENTITY_VISIBLE(player_ped, FALSE, FALSE);
	PLAYER::SET_EVERYONE_IGNORE_PLAYER(player_id, TRUE);
	PLAYER::SET_POLICE_IGNORE_PLAYER(player_id, TRUE);

	// Fix environment for clean data capture
	TIME::SET_CLOCK_TIME(12, 0, 0);
	TIME::PAUSE_CLOCK(TRUE);
	GAMEPLAY::SET_WEATHER_TYPE_NOW_PERSIST(const_cast<char*>("EXTRASUNNY"));
	VEHICLE::SET_RANDOM_TRAINS(FALSE);
	VEHICLE::SET_RANDOM_BOATS(FALSE);
}

// Invincibility, visibility, and environment settings that reset each tick or on respawn.
void MaintainPlayerState()
{
	Player player_id = PLAYER::PLAYER_ID();
	Ped player_ped = PLAYER::PLAYER_PED_ID();
	PLAYER::SET_PLAYER_INVINCIBLE(player_id, TRUE);
	ENTITY::SET_ENTITY_VISIBLE(player_ped, FALSE, FALSE);

	// Density multipliers only apply for the current frame — must be called every tick.
	PED::SET_PED_DENSITY_MULTIPLIER_THIS_FRAME(0.0f);
	PED::SET_SCENARIO_PED_DENSITY_MULTIPLIER_THIS_FRAME(0.0f, 0.0f);
	VEHICLE::SET_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME(0.0f);
	VEHICLE::SET_RANDOM_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME(0.0f);
	VEHICLE::SET_PARKED_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME(0.0f);

	// Re-apply weather each tick to suppress any engine-driven transitions.
	GAMEPLAY::SET_WEATHER_TYPE_NOW_PERSIST(const_cast<char*>("EXTRASUNNY"));
}

// Picks a random map position, teleports the player to the ground there,
// then positions the camera directly above with a random altitude and angle.
void RandomizeDronePosition()
{
	float x, y;
	do {
		x = -300.0f + static_cast<float>(rand()) / (RAND_MAX / 700.0f);
		y = -2000.0f + static_cast<float>(rand()) / (RAND_MAX / 3500.0f);
	} while (y < 1200.0f); // exclude urban areas

	// Load collision and scene data for the new area
	STREAMING::REQUEST_COLLISION_AT_COORD(x, y, 0.0f);
	STREAMING::REQUEST_ADDITIONAL_COLLISION_AT_COORD(x, y, 0.0f);
	STREAMING::LOAD_SCENE(x, y, 0.0f);
	WAIT(2500);

	// Query ground Z from 807 m (summit of Mount Chiliad)
	// Avoid querying from 1000 m: when collision hasn't loaded the function can
	// misreport the query altitude itself as the ground, giving a bogus ~1000 m result.
	float ground_z = 0.0f;
	bool found_ground = false;
	for (int i = 0; i < 20 && !found_ground; i++) {
		found_ground = GAMEPLAY::GET_GROUND_Z_FOR_3D_COORD(x, y, 807.0f, &ground_z, FALSE);
		if (!found_ground) WAIT(100);
	}

	// If the point is over water, use the water surface as the floor
	float water_z = 0.0f;
	if (WATER::GET_WATER_HEIGHT(x, y, ground_z + 1.0f, &water_z)) {
		if (water_z > ground_z) ground_z = water_z;
	}

	float safe_z = found_ground ? ground_z + 1.0f : 30.0f;
	Ped player_ped = PLAYER::PLAYER_PED_ID();
	ENTITY::SET_ENTITY_COORDS_NO_OFFSET(player_ped, x, y, safe_z, FALSE, FALSE, FALSE);
	WAIT(500);

	// Read actual post-teleport player position (may differ slightly from requested x/y)
	Vector3 player_pos = ENTITY::GET_ENTITY_COORDS(player_ped, TRUE);

	g_cam_altitude_offset = 20.0f + static_cast<float>(rand() % 31); // 20–50 m above player

	// Initial camera position; the main loop keeps it tracking the player each frame.
	Vector3 cam_pos;
	cam_pos.x = player_pos.x;
	cam_pos.y = player_pos.y;
	cam_pos.z = player_pos.z + g_cam_altitude_offset;

	Vector3 cam_rot;
	cam_rot.x = -20.0f - static_cast<float>(rand() % 71); // -20 to -90 pitch
	cam_rot.y = 0.0f;
	cam_rot.z = static_cast<float>(rand() % 360);

	g_camera.SetPosition(cam_pos);
	g_camera.SetRotation(cam_rot);
}

// _ADD_TEXT_COMPONENT_STRING truncates at 99 chars per call even with CELL_EMAIL_BCON.
// Feed the text in 99-character chunks to avoid silent truncation.
static void AddLongText(const char* text)
{
	const int LIMIT = 99;
	char chunk[100];
	int len = (int)strlen(text);
	for (int i = 0; i < len; i += LIMIT) {
		int n = len - i;
		if (n > LIMIT) n = LIMIT;
		memcpy(chunk, text + i, n);
		chunk[n] = '\0';
		UI::_ADD_TEXT_COMPONENT_STRING(chunk);
	}
}

void DrawDebugText()
{
	UI::SET_TEXT_FONT(0);
	UI::SET_TEXT_SCALE(0.5f, 0.5f);
	UI::SET_TEXT_COLOUR(255, 255, 255, 255);
	UI::_SET_TEXT_ENTRY("CELL_EMAIL_BCON");

	Vector3 cam_pos = g_camera.IsRendering()
		? g_camera.GetPosition()
		: CAM::GET_GAMEPLAY_CAM_COORD();
	Vector3 cam_rot = g_camera.IsRendering()
		? g_camera.GetRotation()
		: CAM::GET_GAMEPLAY_CAM_ROT(2);
	char status_text[512];
	sprintf_s(status_text, sizeof(status_text),
		"AEROSYNTH Data Capture [%s]\n"
		"F6 - Capture Frame\n"
		"F7 - Toggle Continuous (%s)\n"
		"F8 - Random Drone Position\n"
		"F9 - Toggle Camera Mode\n"
		"Session: %s\n"
		"Pos: %.1f, %.1f, %.1f\n"
		"Rot: p=%.1f r=%.1f y=%.1f\n"
		"Depth: hook=%s omRT=%d match=%d clrDSV=%d fmt=%u %dx%d map=%s%s",
		g_camera.IsRendering() ? "AERIAL" : "NORMAL",
		g_continuous_capture ? "ON" : "OFF",
		g_session_directory.empty() ? "Not Started" : "Active",
		cam_pos.x, cam_pos.y, cam_pos.z,
		cam_rot.x, cam_rot.y, cam_rot.z,
		DepthCapturer::s_hooked        ? "Y" : "N",
		DepthCapturer::s_omSetRTCount,
		DepthCapturer::s_matchCount,
		DepthCapturer::s_dsvCallCount,
		DepthCapturer::s_lastFormat,
		DepthCapturer::s_lastW,
		DepthCapturer::s_lastH,
		DepthCapturer::s_lastMapOk     ? "ok" : "fail",
		DepthCapturer::s_timedOut      ? " TIMEOUT" : "");

	AddLongText(status_text);
	UI::_DRAW_TEXT(0.05f, 0.05f);
}

void CaptureFrame(int frame_number)
{
	CameraIntrinsics intrinsics = g_camera.GetIntrinsics();

	char rgb_path[256];
	sprintf_s(rgb_path, sizeof(rgb_path),
		"%s/frame_%06d.bmp", g_session_directory.c_str(), frame_number);

	if (!g_screen_capturer.CaptureScreenToPNG(rgb_path)) {
		return;
	}

	char depth_path[256];
	sprintf_s(depth_path, sizeof(depth_path),
		"%s/frame_%06d_depth.bmp", g_session_directory.c_str(), frame_number);
	bool depth_ok = g_depth_capturer.SaveDepth(depth_path, intrinsics.near_clip, intrinsics.far_clip);

	char seg_path[256];
	sprintf_s(seg_path, sizeof(seg_path),
		"%s/frame_%06d_seg.bmp", g_session_directory.c_str(), frame_number);
	if (depth_ok)
		g_depth_capturer.SaveSegmentation(seg_path);

	FrameData frame_data;
	frame_data.frame_number      = frame_number;
	frame_data.timestamp         = (long long)time(nullptr) * 1000;
	frame_data.resolution.width  = g_screen_capturer.GetScreenWidth();
	frame_data.resolution.height = g_screen_capturer.GetScreenHeight();
	frame_data.extrinsics        = g_camera.GetExtrinsics();
	frame_data.intrinsics        = intrinsics;

	char rgb_filename[64], depth_filename[64], seg_filename[64];
	sprintf_s(rgb_filename,   sizeof(rgb_filename),   "frame_%06d.bmp",       frame_number);
	sprintf_s(depth_filename, sizeof(depth_filename), "frame_%06d_depth.bmp", frame_number);
	sprintf_s(seg_filename,   sizeof(seg_filename),   "frame_%06d_seg.bmp",   frame_number);
	frame_data.rgb_filename   = rgb_filename;
	frame_data.depth_filename = depth_filename;
	if (depth_ok)
		frame_data.segmentation_filename = seg_filename;

	// Right stereo capture — shift camera 5 m along its horizontal right axis,
	// grab RGB, then restore. No depth/seg needed for the right eye.
	static const float kStereoBaseline = 1.0f;
	Vector3 left_pos  = g_camera.GetPosition();
	Vector3 right_pos = g_camera.GetStereoRightPosition(kStereoBaseline);
	g_camera.SetPosition(right_pos);
	WAIT(0);  // GTA V renders right frame
	WAIT(0);  // DWM presents it — BitBlt now reads the right frame

	char right_rgb_path[256], right_rgb_filename[64];
	sprintf_s(right_rgb_path,     sizeof(right_rgb_path),     "%s/frame_%06d_right.bmp", g_session_directory.c_str(), frame_number);
	sprintf_s(right_rgb_filename, sizeof(right_rgb_filename), "frame_%06d_right.bmp",    frame_number);

	if (g_screen_capturer.CaptureScreenToPNG(right_rgb_path)) {
		frame_data.right_rgb_filename       = right_rgb_filename;
		frame_data.right_extrinsics.position = right_pos;
		frame_data.right_extrinsics.rotation = g_camera.GetRotation();
	}

	g_camera.SetPosition(left_pos);

	g_file_exporter.ExportFrame(frame_data, rgb_path);
}

void ProcessHotkeys()
{
	// F6 - Capture single frame
	if (IsKeyJustUp(0x75)) { // 0x75 = F6
		if (!g_capture_enabled) {
			g_capture_enabled = true;
			g_session_directory = g_file_exporter.InitializeSession();
		}

		if (!g_session_directory.empty()) {
			CaptureFrame(frame_count++);
		}
	}

	// F8 - Teleport to a random drone-like position
	if (IsKeyJustUp(0x77)) { // 0x77 = F8
		RandomizeDronePosition();
	}

	// F9 - Toggle between aerial scripted camera and normal game camera
	if (IsKeyJustUp(0x78)) { // 0x78 = F9
		g_camera.SetRendering(!g_camera.IsRendering());
	}
}

void main()
{
	// Wait until the player ped is fully spawned and in control
	while (!PLAYER::IS_PLAYER_CONTROL_ON(PLAYER::PLAYER_ID())) WAIT(0);

	srand(static_cast<unsigned>(time(nullptr)));
	SetupPlayer();

	// Place the initial camera directly above the player's actual spawn position
	Vector3 player_pos = ENTITY::GET_ENTITY_COORDS(PLAYER::PLAYER_PED_ID(), TRUE);
	Vector3 start_pos;
	start_pos.x = player_pos.x;
	start_pos.y = player_pos.y;
	start_pos.z = player_pos.z + 100.0f;

	Vector3 start_rot;
	start_rot.x = -45.0f;
	start_rot.y = 0.0f;
	start_rot.z = 0.0f;

	g_camera.CreateCamera(start_pos, start_rot, 50.0f);

	while (true)
	{
		// Process hotkeys
		ProcessHotkeys();

		// F7 - enter continuous capture loop
		if (IsKeyJustUp(0x76)) {
			if (!g_capture_enabled) {
				g_capture_enabled = true;
				g_session_directory = g_file_exporter.InitializeSession();
			}
			if (!g_session_directory.empty()) {
				g_continuous_capture = true;
				UI::DISPLAY_HUD(FALSE);
				UI::DISPLAY_RADAR(FALSE);

				while (g_continuous_capture) {
					RandomizeDronePosition();

					// 10-second settle: maintain state and camera tracking each frame
					for (int i = 0; i < 200 && g_continuous_capture; i++) {
						if (g_camera.IsRendering()) {
							Vector3 pp = ENTITY::GET_ENTITY_COORDS(PLAYER::PLAYER_PED_ID(), TRUE);
							Vector3 cp; cp.x = pp.x; cp.y = pp.y; cp.z = pp.z + g_cam_altitude_offset;
							g_camera.SetPosition(cp);
						}
						MaintainPlayerState();
						if (IsKeyJustUp(0x76)) g_continuous_capture = false;
						WAIT(50);
					}

					if (g_continuous_capture) {
						CaptureFrame(frame_count++);
					}
				}

				UI::DISPLAY_HUD(TRUE);
				UI::DISPLAY_RADAR(TRUE);
			}
		}

		// Keep aerial camera directly above the player (X/Y only; Z and rotation are fixed).
		if (g_camera.IsRendering()) {
			Vector3 player_pos = ENTITY::GET_ENTITY_COORDS(PLAYER::PLAYER_PED_ID(), TRUE);
			Vector3 cam_pos;
			cam_pos.x = player_pos.x;
			cam_pos.y = player_pos.y;
			cam_pos.z = player_pos.z + g_cam_altitude_offset;
			g_camera.SetPosition(cam_pos);
		}

		// Re-apply invincibility and invisibility in case of respawn
		MaintainPlayerState();

		// Draw debug UI
		DrawDebugText();

		WAIT(0);
	}
}

void ScriptMain()
{
	main();
}