/*
	THIS FILE IS A PART OF GTA V SCRIPT HOOK SDK
				http://dev-c.com			
			(C) Alexander Blade 2015
*/

/*
	AEROSYNTH Data Capture Script
	Hotkeys:
	- 'C' (key 67): Capture single frame
	- 'V' (key 86): Toggle continuous capture on/off
	- Arrow keys: Move camera (if in manual mode)
*/

#include "script.h"
#include "keyboard.h"
#include "screencapturer.h"
#include "camera.h"
#include "fileexporter.h"
#include <windows.h>
#include <cstdio>
#include <ctime>

// Global objects
ScreenCapturer g_screen_capturer;
ScriptedCamera g_camera;
FileExporter g_file_exporter;

// State variables
bool g_capture_enabled = false;
bool g_continuous_capture = false;
unsigned int g_last_capture_time = 0;
unsigned int g_capture_interval_ms = 50; // ~20 fps for continuous capture
std::string g_session_directory = "";

void DrawDebugText()
{
	UI::SET_TEXT_FONT(0);
	UI::SET_TEXT_SCALE(0.5f, 0.5f);
	UI::SET_TEXT_COLOUR(255, 255, 255, 255);
	UI::_SET_TEXT_ENTRY("STRING");

	char status_text[256];
	sprintf_s(status_text, sizeof(status_text),
		"AEROSYNTH Data Capture\n"
		"C - Capture Frame\n"
		"V - Toggle Continuous (%s)\n"
		"Session: %s",
		g_continuous_capture ? "ON" : "OFF",
		g_session_directory.empty() ? "Not Started" : "Active");

	UI::_ADD_TEXT_COMPONENT_STRING(status_text);
	UI::_DRAW_TEXT(0.05f, 0.05f);
}

void CaptureFrame(int frame_number)
{
	// Capture screen to temporary BMP
	char temp_image_path[256];
	sprintf_s(temp_image_path, sizeof(temp_image_path),
		"%s/frame_%06d.bmp", g_session_directory.c_str(), frame_number);

	if (!g_screen_capturer.CaptureScreenToPNG(temp_image_path)) {
		return;
	}

	// Prepare frame data
	FrameData frame_data;
	frame_data.frame_number = frame_number;
	frame_data.timestamp = (long long)time(nullptr) * 1000; // ms
	frame_data.resolution.width = g_screen_capturer.GetScreenWidth();
	frame_data.resolution.height = g_screen_capturer.GetScreenHeight();

	// Get camera parameters
	frame_data.extrinsics = g_camera.GetExtrinsics();
	frame_data.intrinsics = g_camera.GetIntrinsics();

	// Set filenames
	char rgb_filename[64];
	sprintf_s(rgb_filename, sizeof(rgb_filename), "frame_%06d.bmp", frame_number);
	frame_data.rgb_filename = rgb_filename;

	// Export frame metadata
	g_file_exporter.ExportFrame(frame_data, temp_image_path);
}

void ProcessHotkeys()
{
	// 'C' key - Capture single frame
	if (IsKeyJustUp(0x43)) { // 0x43 = 'C'
		if (!g_capture_enabled) {
			g_capture_enabled = true;
			g_session_directory = g_file_exporter.InitializeSession();
		}

		if (!g_session_directory.empty()) {
			CaptureFrame(g_file_exporter.GetSessionDirectory().empty() ? 0 : 1);
		}
	}

	// 'V' key - Toggle continuous capture
	if (IsKeyJustUp(0x56)) { // 0x56 = 'V'
		if (!g_capture_enabled) {
			g_capture_enabled = true;
			g_session_directory = g_file_exporter.InitializeSession();
		}

		g_continuous_capture = !g_continuous_capture;
		g_last_capture_time = GetTickCount();
	}
}

void main()
{
	// Initialize camera at a starting location
	Vector3 start_pos;
	start_pos.x = 0.0f;
	start_pos.y = 0.0f;
	start_pos.z = 100.0f;
	
	Vector3 start_rot;
	start_rot.x = 0.0f;
	start_rot.y = 0.0f;
	start_rot.z = 0.0f;
	
	g_camera.CreateCamera(start_pos, start_rot, 50.0f);

	int frame_count = 0;

	while (true)
	{
		// Process hotkeys
		ProcessHotkeys();

		// Continuous capture: capture every N milliseconds
		if (g_continuous_capture && !g_session_directory.empty()) {
			unsigned int current_time = GetTickCount();
			if (current_time - g_last_capture_time >= g_capture_interval_ms) {
				CaptureFrame(frame_count++);
				g_last_capture_time = current_time;
			}
		}

		// Draw debug UI
		DrawDebugText();

		WAIT(0);
	}
}

void ScriptMain()
{
	main();
}