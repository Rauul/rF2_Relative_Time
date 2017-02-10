/*
rF2 Relative Time

Author: Martin Vindis <martin.vindis.80@gmail.com>
Date:   2016/2017
 
*/

using namespace std;

#ifndef _INTERNALS_EXAMPLE_H
#define _INTERNALS_EXAMPLE_H

#include "InternalsPlugin.hpp"
#include <algorithm>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <d3dx9.h>
#include <Windows.h>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>

#define PLUGIN_NAME             "rF2_RelativeTime"
#define PLUGIN_VERSION			"1.16"

#define  ENABLE_LOG              false

#if _WIN64
	#define LOG_FILE				"Bin64\\Plugins\\RelativeTime.log"
	#define CONFIG_FILE				"Bin64\\Plugins\\RelativeTime.ini"
	#define TEXTURE_BACKGROUND		"Bin64\\Plugins\\RelativeTimeBackground.png"
	#define MOUSE_TEXTURE			"Bin64\\Plugins\\RelativeTimeCursor.png"
#else
	#define LOG_FILE				"Bin32\\Plugins\\RelativeTime.log"
	#define CONFIG_FILE				"Bin32\\Plugins\\RelativeTime.ini"
	#define TEXTURE_BACKGROUND		"Bin32\\Plugins\\RelativeTimeBackground.png"
	#define MOUSE_TEXTURE			"Bin32\\Plugins\\RelativeTimeCursor.png"
#endif

#define COLOR_INTENSITY				0xF0

#define SMALL_FONT_NAME				"Arial"
#define SMALL_FONT_SIZE				16
#define BIG_FONT_NAME				"Arial"
#define BIG_FONT_SIZE				38
#define GRID_FONT_NAME				"Tahoma"
#define GRID_FONT_SIZE				20

#define USE_BORDERS					true
#define BACKGROUND_COLOR			0xFF000000

#define PLAYER_ON_TRACK_COLOR		0xFFd6a21f
#define PLAYER_IN_PITS_COLOR		0xFFaa8119
#define EQUAL_ON_TRACK_COLOR		0xFFe6e6e6
#define EQUAL_IN_PITS_COLOR			0xFF808080
#define FASTER_ON_TRACK_COLOR		0xFFd95656
#define FASTER_IN_PITS_COLOR		0xFF933a3a
#define SLOWER_ON_TRACK_COLOR		0xFF2c88ce
#define SLOWER_IN_PITS_COLOR		0xFF1b547f

#define DEFAULT_EDIT_KEY			(0x45)      /* "E" */
#define KEY_DOWN(k)					((GetAsyncKeyState(k) & 0x8000) && (GetAsyncKeyState(VK_CONTROL) & 0x8000))

#define FONT_NAME_MAXLEN			32

#define DEFAULT_WIDGET_POSITION		0
#define DEFAULT_WIDGET_SIZE			100
#define DEFAULT_WIDGET_ENABLED		true

// This is used for the app to use the plugin for its intended purpose
class rF2_Relative : public InternalsPluginV06
{

public:

	// Constructor/destructor
	rF2_Relative() {}
	~rF2_Relative() {}

	// These are the functions derived from base class InternalsPlugin
	// that can be implemented.
	void Startup(long version);    // game startup
	void Shutdown() {};            // game shutdown

	void EnterRealtime();          // entering realtime
	void ExitRealtime();           // exiting realtime

	void StartSession();           // session has started
	void EndSession();             // session has ended

	void Load();                   // when a new track/car is loaded
	void Unload();                 // back to the selection screen

	// GAME OUTPUT
	long WantsTelemetryUpdates() { return 1; }

	/* 1 = Player only */
	void UpdateTelemetry(const TelemInfoV01 &tele);

	bool WantsGraphicsUpdates() { return false; }
	void UpdateGraphics(const GraphicsInfoV02 &info) { }

	//bool WantsToDisplayMessage(MessageInfoV01 &msgInfo);

	// GAME INPUT
	bool HasHardwareInputs() { return false; }
	void UpdateHardware(const float fDT) { mET += fDT; }   // update the hardware with the time between frames
	void EnableHardware() { mEnabled = true; }             // message from game to enable hardware
	void DisableHardware() { mEnabled = false; }           // message from game to disable hardware

	// See if the plugin wants to take over a hardware control.  If the plugin takes over the
	// control, this method returns true and sets the value of the float pointed to by the
	// second arg.  Otherwise, it returns false and leaves the float unmodified.
	bool CheckHWControl(const char * const controlName, float &fRetVal);
	bool ForceFeedback(float &forceValue) { return false; }

	// SCORING OUTPUT
	bool WantsScoringUpdates() { return true; }
	void UpdateScoring(const ScoringInfoV01 &info);

	// COMMENTARY INPUT
	bool RequestCommentary(CommentaryRequestInfoV01 &info) { return false; }

	// VIDEO EXPORT (sorry, no example code at this time)
	virtual bool WantsVideoOutput() { return false; }
	virtual bool VideoOpen(const char * const szFilename, float fQuality, unsigned short usFPS, unsigned long fBPS,
		unsigned short usWidth, unsigned short usHeight, char *cpCodec = NULL) {
		return(false);
	} // open video output file
	virtual void VideoClose() {}                                 // close video output file
	virtual void VideoWriteAudio(const short *pAudio, unsigned int uNumFrames) {} // write some audio info
	virtual void VideoWriteImage(const unsigned char *pImage) {} // write video image

	// SCREEN NOTIFICATIONS

	void InitScreen(const ScreenInfoV01 &info);                  // Now happens right after graphics device initialization
	void UninitScreen(const ScreenInfoV01 &info);                // Now happens right before graphics device uninitialization

	void DeactivateScreen(const ScreenInfoV01 &info);            // Window deactivation
	void ReactivateScreen(const ScreenInfoV01 &info);            // Window reactivation

	void RenderScreenBeforeOverlays(const ScreenInfoV01 &info);  // before rFactor overlays
	void RenderScreenAfterOverlays(const ScreenInfoV01 &info);   // after rFactor overlays

	void PreReset(const ScreenInfoV01 &info);					 // after detecting device lost but before resetting
	void PostReset(const ScreenInfoV01 &info);					 // after resetting


	void ThreadStarted(long type) {}                             // called just after a primary thread is started (type is 0=multimedia or 1=simulation)
	void ThreadStopping(long type) {}                            // called just before a primary thread is stopped (type is 0=multimedia or 1=simulation)

private:

	bool WantsToDisplayMessage(MessageInfoV01 & msgInfo);

	void DrawGraphics(const ScreenInfoV01 &info);
	void LoadConfig(struct PluginConfig &config, struct GridConfig &grid, const char *ini_file);
	void SaveConfig(struct GridConfig &grid, const char * ini_file);
	void DrawGridWidget(D3DCOLOR text_color, D3DCOLOR shadow_color);
	void ReadUserDefinedClassColors();
	D3DCOLOR GetGridTextColor(int other, int player);
	bool NeedToDisplay();
	void WriteLog(const char * const msg);

	//
	// Current status
	//

	float mET;                          /* needed for the hardware example */
	bool mEnabled;                      /* needed for the hardware example */


	//
	// User variables
	//
	D3DCOLOR PlayerOnTrackColor = 0xFFd6a21f;
	D3DCOLOR PlayerInPitsColor = 0xFFaa8119;
	D3DCOLOR EqualCarOnTrackColor = 0xFFe6e6e6;
	D3DCOLOR EqualCarInPitsColor = 0xFF808080;
	D3DCOLOR FasterCarOnTrackColor = 0xFFd95656;
	D3DCOLOR FasterCarInPitsColor = 0xFF933a3a;
	D3DCOLOR SlowerCarOnTrackColor = 0xFF2c88ce;
	D3DCOLOR SlowerCarInPitsColor = 0xFF1b547f;

	vector<string> userClassColorKeys;
	vector<D3DCOLOR> userClassColorValues;
};

inline int roundi(float x) { return int(floor(x + 0.5)); }

#endif // _INTERNALS_EXAMPLE_H