/*
rF2 Relative Time

Author: Martin Vindis <martin.vindis.80@gmail.com>
Date:   2016/2017

*/


#include "rF2_Relative.hpp"
#include "Driver.hpp"

// plugin information

extern "C" __declspec(dllexport)
const char * __cdecl GetPluginName() { return PLUGIN_NAME; }

extern "C" __declspec(dllexport)
PluginObjectType __cdecl GetPluginType() { return PO_INTERNALS; }

extern "C" __declspec(dllexport)
int __cdecl GetPluginVersion() { return 6; }

extern "C" __declspec(dllexport)
PluginObject * __cdecl CreatePluginObject() { return((PluginObject *) new rF2_Relative); }

extern "C" __declspec(dllexport)
void __cdecl DestroyPluginObject(PluginObject *obj) { delete((rF2_Relative *)obj); }

POINT default_widget_pos;
bool devmode = false;
bool in_edit_mode = false;
bool is_multi_class = false;
bool in_realtime = false;              /* Are we in cockpit? As opposed to monitor */
bool in_race = false;
bool local_player_in_control = false;
bool displayed_welcome = false;        /* Whether we displayed the "plugin enabled" welcome message */
bool key_down_last_frame = false;
bool editkey_down_last_frame = false;
D3DXVECTOR2 resolution;
vector<Driver> drivers;
vector<string> vehicleClasses;
D3DCOLOR CLASS_COLORS[100] = { 0xFFd9ba00, 0xFFb40ed9, 0xFF3da5a3, 0xFFf26d7d, 0xFF72f6ff, 0xFFc9d17f, 0xFF7ad893, 0xFFda6d2d, 0xFFc58516, 0xFFf3d9f2, 0xFFd32c10, 0xFF9d1d71,
0xFF1c0bba, 0xFFe6589c, 0xFF0a3af6, 0xFF4cf556, 0xFF579b9b, 0xFFc06fac, 0xFFe53c26, 0xFF4e9440, 0xFF50ffec, 0xFFa9a16f, 0xFFd8e2a2, 0xFF46cee0, 0xFF750119, 0xFF16b873,
0xFF073685, 0xFFb25f80, 0xFF67a6d0, 0xFFbd0288, 0xFF7d7149, 0xFF699d1f, 0xFFf6738e, 0xFFc0773d, 0xFF7b3aa5, 0xFFfaa167, 0xFF76001f, 0xFFdb5210, 0xFF6fa381, 0xFF6ae8a0,
0xFF834df6, 0xFF6f34af, 0xFFd5be24, 0xFFad6251, 0xFFa78fdd, 0xFF498419, 0xFF24eeba, 0xFFeca3b9, 0xFFc56498, 0xFFb46f6d, 0xFF3ebf3e, 0xFF4796e6, 0xFFd04343 };

enum MouseLocked
{
	None,
	Fuel,
	Grid
};

struct GridConfig
{
	bool enabled = true;
	int mode;

	double sizeMultiplier = 1.0;

	RECT rect;
	D3DXVECTOR3 pos;
	D3DXVECTOR2 offset;
} grid;

struct PluginConfig
{
	unsigned int big_font_size;
	unsigned int small_font_size;
	unsigned int grid_font_size;
	unsigned int editkey;

	bool use_borders;
	D3DCOLOR background_color;

	char big_font_name[FONT_NAME_MAXLEN];
	char small_font_name[FONT_NAME_MAXLEN];
	char grid_font_name[FONT_NAME_MAXLEN];
} config;


// Things idk where to put 
MouseLocked MouseLockedTo = MouseLocked::None;

// DirectX 9 objects, to render some text on screen
LPD3DXFONT big_Font = NULL;
D3DXFONT_DESC BigFontDesc = {
	BIG_FONT_SIZE, 0, 400, 0, false, DEFAULT_CHARSET,
	OUT_TT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH, BIG_FONT_NAME
};
LPD3DXFONT small_Font = NULL;
D3DXFONT_DESC SmallFontDesc = {
	SMALL_FONT_SIZE, 0, 400, 0, false, DEFAULT_CHARSET,
	OUT_TT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH, SMALL_FONT_NAME
};
LPD3DXFONT grid_Font = NULL;
D3DXFONT_DESC GridFontDesc = {
	GRID_FONT_SIZE, 0, 700, 0, false, DEFAULT_CHARSET,
	OUT_TT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH, GRID_FONT_NAME
};
RECT DataPosition, DataShadowPosition, LabelPosition, LabelShadowPosition;
LPD3DXSPRITE background = NULL;
LPD3DXSPRITE classBox = NULL;
LPD3DXSPRITE cursor = NULL;
LPDIRECT3DTEXTURE9 texture = NULL;
LPDIRECT3DTEXTURE9 mouse_texture = NULL;

//
// rF2_Relative class
//

void rF2_Relative::WriteLog(const char * const msg)
{
	FILE *fo;
	fo = fopen(LOG_FILE, "a");
	if (fo != NULL)
		fprintf(fo, msg);
	fclose(fo);
}

void rF2_Relative::Startup(long version)
{
	// default HW control enabled to true
	mEnabled = true;
	if (ENABLE_LOG)
		WriteLog("--STARTUP--\n");

	ReadUserDefinedClassColors();
}

void rF2_Relative::StartSession()
{
	if (ENABLE_LOG)
		WriteLog("--STARTSESSION--");
}

void rF2_Relative::EndSession()
{
	if (ENABLE_LOG)
		WriteLog("--ENDSESSION--");

	mET = 0.0f;
	in_realtime = false;
}

void rF2_Relative::Load()
{
	if (ENABLE_LOG)
		WriteLog("--LOAD--");
}

void rF2_Relative::Unload()
{
	if (ENABLE_LOG)
		WriteLog("--UNLOAD--");
}

// Find the next token in a null-delimited series of strings.
// Example: "a=1234\0b=2345\0c=ABCD\0\0".
// Null bytes ("\0") delimit the strings.
// The whole buffer is terminated by a double null byte ("\0\0").
// strchr is a handy tool to search for next null byte.
// Example usage:
//		for ( LPCSTR pToken = pBuffer; pToken && *pToken; pToken = NextToken(pToken) )
//		{
//			puts( pToken );
//		}
// Use in conjunction with Windows API ::GetPrivateProfileSection
static inline LPCSTR NextToken(LPCSTR pArg)
{
	// find next null with strchr and
	// point to the char beyond that.
	return strchr(pArg, '\0') + 1;
}

void rF2_Relative::EnterRealtime()
{
	if (ENABLE_LOG)
		WriteLog("---ENTERREALTIME---");

	// start up timer every time we enter realtime
	mET = 0.0f;
	in_realtime = true;
	in_edit_mode = false;
}

void rF2_Relative::ExitRealtime()
{
	if (ENABLE_LOG)
		WriteLog("---EXITREALTIME---");
	in_realtime = false;
	SaveConfig(grid, CONFIG_FILE);
}

bool rF2_Relative::NeedToDisplay()
{
	// If we're in the monitor or replay,
	// no delta best should be displayed
	if (!in_realtime || !local_player_in_control)
		return false;

	return true;
}

int GetPlayerArrayPos()
{
	for (int i = 0; i < drivers.size(); i++)
	{
		if (drivers[i].control == 0 || drivers[i].isPlayer)
			return i;
	}
	return 0;
}

void rF2_Relative::UpdateScoring(const ScoringInfoV01 &info)
{
	in_realtime = info.mInRealtime;
	/* No scoring updates should take place if we're
	in the monitor as opposed to the cockpit mode */
	if (!info.mInRealtime)
		return;

	in_race = info.mSession >= 10;

	drivers.clear();
	int playerArrayPos;

	local_player_in_control = false;

	for (long i = 0; i < info.mNumVehicles; ++i)
	{
		VehicleScoringInfoV01 &vinfo = info.mVehicle[i];

		Driver driver(vinfo.mDriverName, vinfo.mVehicleClass, vinfo.mTotalLaps, vinfo.mNumPitstops, vinfo.mLapDist, info.mLapDist, vinfo.mBestLapTime, vinfo.mLastLapTime,
			vinfo.mTimeIntoLap, vinfo.mEstimatedLapTime, vinfo.mLocalVel.z, vinfo.mInPits, vinfo.mIsPlayer, vinfo.mControl, vinfo.mFinishStatus, vinfo.mPlace, vinfo.mPitState,
			vinfo.mTimeBehindLeader);

		drivers.push_back(driver);

		// If this isn't the player car then there is no point going on
		if (vinfo.mControl > 1)
			continue;

		local_player_in_control = true;

		// We need this later
		playerArrayPos = (int)drivers.size() - 1;
	}

	// If we're not in control there is no point going on
	if (!local_player_in_control)
		return;

	// Calculate the relative distance to the player
	for (int i = 0; i < (int)drivers.size(); i++)
	{
		if (drivers[i].control == 0 || drivers[i].isPlayer)
		{
			drivers[i].relativeDistance = 0.0;
			drivers[i].relativeTime = 0.0;
			continue;
		}

		if (drivers[playerArrayPos].lapDistance < info.mLapDist / 2)
		{
			if (drivers[i].lapDistance < drivers[playerArrayPos].lapDistance || drivers[i].lapDistance < drivers[playerArrayPos].lapDistance + info.mLapDist / 2)
				drivers[i].relativeDistance = drivers[playerArrayPos].lapDistance - drivers[i].lapDistance;
			else
				drivers[i].relativeDistance = info.mLapDist - drivers[i].lapDistance + drivers[playerArrayPos].lapDistance;
		}
		else
			if (drivers[i].lapDistance > drivers[playerArrayPos].lapDistance || drivers[i].lapDistance > drivers[playerArrayPos].lapDistance - info.mLapDist / 2)
			{
				drivers[i].relativeDistance = drivers[playerArrayPos].lapDistance - drivers[i].lapDistance;
			}
			else
			{
				drivers[i].relativeDistance = -abs(drivers[playerArrayPos].lapDistance - info.mLapDist - drivers[i].lapDistance);
			}

		double truetime = drivers[i].estimatedLapTime * (drivers[playerArrayPos].timeIntoLap / drivers[playerArrayPos].estimatedLapTime);


		// Calculate the reative time to driver
		if (drivers[playerArrayPos].timeIntoLap < drivers[playerArrayPos].estimatedLapTime / 2)
		{
			if (drivers[i].timeIntoLap < drivers[playerArrayPos].timeIntoLap || drivers[i].timeIntoLap < drivers[playerArrayPos].timeIntoLap + drivers[i].estimatedLapTime / 2)
				drivers[i].relativeTime = drivers[playerArrayPos].timeIntoLap - drivers[i].timeIntoLap;
			else
				drivers[i].relativeTime = drivers[i].estimatedLapTime - drivers[i].timeIntoLap + drivers[playerArrayPos].timeIntoLap;
		}
		else
			if (drivers[i].timeIntoLap > drivers[playerArrayPos].timeIntoLap || drivers[i].timeIntoLap > drivers[playerArrayPos].timeIntoLap - drivers[i].estimatedLapTime / 2)
			{
				drivers[i].relativeTime = drivers[playerArrayPos].timeIntoLap - drivers[i].timeIntoLap;
			}
			else
			{
				drivers[i].relativeTime = -abs(drivers[playerArrayPos].timeIntoLap - drivers[playerArrayPos].estimatedLapTime - drivers[i].timeIntoLap);
			}
	}


	// Create class vectors
	vehicleClasses.clear();
	for (int i = 0; i < (int)drivers.size(); i++)
	{
		if (find(vehicleClasses.begin(), vehicleClasses.end(), drivers[i].vehicleClass) == vehicleClasses.end())
			vehicleClasses.push_back(drivers[i].vehicleClass);
	}

	is_multi_class = vehicleClasses.size() > 1;

	std::sort(drivers.begin(), drivers.end(), [](const Driver& lhs, const Driver& rhs) { return lhs.place < rhs.place; });
	for (int i = 0; i < (int)vehicleClasses.size(); i++)
	{
		int placeInClass = 1;
		for (int j = 0; j < (int)drivers.size(); j++)
		{
			if (drivers[j].vehicleClass == vehicleClasses[i])
			{
				drivers[j].placeInClass = placeInClass;
				drivers[j].classColor = i;
				placeInClass++;
			}
		}
	}

	//if (drivers[playerArrayPos].totalLapDistance > 0)
	std::sort(drivers.begin(), drivers.end(), [](const Driver& lhs, const Driver& rhs) { return lhs.relativeTime < rhs.relativeTime; });


	// DEBUG STUFF SET TO FALSE BEFORE RELASE
	if (devmode)
	{
		FILE *fo;

		fo = fopen("VUITest.txt", "w+");
		if (fo != NULL)
		{
			for (int i = 0; i < vehicleClasses.size(); i++)
			{
				fprintf(fo, "Class: %s\n", vehicleClasses[i]);
			}
			for each (Driver driver in drivers)
			{
				fprintf(fo, "Name: %s\n", driver.driverName);
				fprintf(fo, "Lap Distans: %f\n", driver.lapDistance);
				fprintf(fo, "Relativ Distans: %f\n", driver.relativeDistance);
				fprintf(fo, "Relativ Time: %.3f\n", driver.relativeTime);
				fprintf(fo, "Total Laps: %d\n", driver.totalLaps);
				fprintf(fo, "Pct Lap Distans: %f\n", driver.lapDistance / info.mLapDist * 100);
				fprintf(fo, "Exact Laps: %f\n", driver.totalLaps + (driver.lapDistance / info.mLapDist));
				fprintf(fo, "Full Lap Distans: %f\n", driver.fullLapDistance);
				fprintf(fo, "Class: %s\n", driver.vehicleClass);
				fprintf(fo, "Class Color: %d\n", driver.classColor);
				fprintf(fo, "Pitstops: %d\n", driver.numPitstops);
				fprintf(fo, "Best Lap: %f\n", driver.bestLapTime);
				fprintf(fo, "Last Lap: %f\n", driver.lastLapTime);
				fprintf(fo, "Time Into Lap: %f\n", driver.timeIntoLap);
				fprintf(fo, "Est. Lap Time: %f\n", driver.estimatedLapTime);
				fprintf(fo, "In Pits: %d\n", driver.inPits);
				fprintf(fo, "Is Player: %d\n", driver.isPlayer);
				fprintf(fo, "Finish Status: %d\n", driver.finishStatus);
				fprintf(fo, "Pit State: %d\n", driver.pitState);
				fprintf(fo, "Control: %d\n", driver.control);
				fprintf(fo, "Place: %d\n", driver.place);
				fprintf(fo, "Place in class: %d\n\n", driver.placeInClass);
			}
		}
		fclose(fo);
	}
}

void rF2_Relative::UpdateTelemetry(const TelemInfoV01 &tele)
{
	if (!in_realtime)
		return;

	if (KEY_DOWN(config.editkey))
	{
		if (!editkey_down_last_frame)
		{
			in_edit_mode = !in_edit_mode;
			if (!in_edit_mode)
				SaveConfig(grid, CONFIG_FILE);
		}
		editkey_down_last_frame = true;
	}
	else
	{
		editkey_down_last_frame = false;
	}
}

void rF2_Relative::InitScreen(const ScreenInfoV01& info)
{
	if (ENABLE_LOG)
		WriteLog("---INIT SCREEN---\n");

	float *res_x = &resolution.x;
	float *res_y = &resolution.y;

	resolution.x = info.mWidth;
	resolution.y = info.mHeight;

	const float SCREEN_WIDTH = info.mWidth;
	const float SCREEN_HEIGHT = info.mHeight;
	const float ASSUMED_SINGLE_SCREEN_WIDTH = SCREEN_HEIGHT / 0.5625;
	const float SCREEN_CENTER = SCREEN_WIDTH / 2.0;

	LoadConfig(config, grid, CONFIG_FILE);

	grid.pos.x = grid.offset.x + SCREEN_CENTER;
	grid.pos.y = grid.offset.y + SCREEN_HEIGHT / 2;

	BigFontDesc.Height = config.big_font_size;
	sprintf(BigFontDesc.FaceName, config.big_font_name);
	SmallFontDesc.Height = config.small_font_size;
	sprintf(SmallFontDesc.FaceName, config.small_font_name);
	GridFontDesc.Height = config.grid_font_size * grid.sizeMultiplier;
	sprintf(GridFontDesc.FaceName, config.grid_font_name);

	D3DXCreateFontIndirect((LPDIRECT3DDEVICE9)info.mDevice, &BigFontDesc, &big_Font);
	assert(big_Font != NULL);

	D3DXCreateFontIndirect((LPDIRECT3DDEVICE9)info.mDevice, &SmallFontDesc, &small_Font);
	assert(small_Font != NULL);

	D3DXCreateFontIndirect((LPDIRECT3DDEVICE9)info.mDevice, &GridFontDesc, &grid_Font);
	assert(grid_Font != NULL);

	D3DXCreateTextureFromFile((LPDIRECT3DDEVICE9)info.mDevice, TEXTURE_BACKGROUND, &texture);
	D3DXCreateSprite((LPDIRECT3DDEVICE9)info.mDevice, &background);

	D3DXCreateTextureFromFile((LPDIRECT3DDEVICE9)info.mDevice, TEXTURE_BACKGROUND, &texture);
	D3DXCreateSprite((LPDIRECT3DDEVICE9)info.mDevice, &classBox);

	D3DXCreateTextureFromFile((LPDIRECT3DDEVICE9)info.mDevice, MOUSE_TEXTURE, &mouse_texture);
	D3DXCreateSprite((LPDIRECT3DDEVICE9)info.mDevice, &cursor);

	assert(texture != NULL);
	assert(background != NULL);
	assert(classBox != NULL);
	assert(mouse_texture != NULL);
	assert(cursor != NULL);
}

void rF2_Relative::UninitScreen(const ScreenInfoV01& info)
{
	if (big_Font) {
		big_Font->Release();
		big_Font = NULL;
	}
	if (small_Font) {
		small_Font->Release();
		small_Font = NULL;
	}
	if (grid_Font) {
		grid_Font->Release();
		grid_Font = NULL;
	}
	if (background) {
		background->Release();
		background = NULL;
	}
	if (texture) {
		texture->Release();
		texture = NULL;
	}
	if (classBox) {
		classBox->Release();
		classBox = NULL;
	}
	if (cursor) {
		cursor->Release();
		cursor = NULL;
	}
	if (mouse_texture) {
		mouse_texture->Release();
		mouse_texture = NULL;
	}
}

void rF2_Relative::DeactivateScreen(const ScreenInfoV01& info)
{
}

void rF2_Relative::ReactivateScreen(const ScreenInfoV01& info)
{
}

bool MouseIsInBox(POINT mouse_pos, D3DXVECTOR3 rect_pos, RECT rect)
{
	if (mouse_pos.y >= rect_pos.y && mouse_pos.y <= rect_pos.y + rect.bottom && mouse_pos.x >= rect_pos.x && mouse_pos.x <= rect_pos.x + rect.right)
		return true;
	else
		return false;
}

void DrawBox(D3DCOLOR border_color, D3DXVECTOR3 pos, RECT rect)
{
	// Draw background
	background->Begin(D3DXSPRITE_ALPHABLEND);
	background->Draw(texture, &rect, NULL, &pos, config.background_color);
	background->End();

	// Draw border
	if (config.use_borders || in_edit_mode)
	{
		RECT border_rect = rect;
		D3DXVECTOR3 border_rect_pos = pos;

		// Top
		background->Begin(D3DXSPRITE_ALPHABLEND);
		border_rect.bottom = 1;
		background->Draw(texture, &border_rect, NULL, &border_rect_pos, border_color);
		background->End();

		// Bottom
		background->Begin(D3DXSPRITE_ALPHABLEND);
		border_rect_pos.y += rect.bottom;
		background->Draw(texture, &border_rect, NULL, &border_rect_pos, border_color);
		background->End();

		// Left
		background->Begin(D3DXSPRITE_ALPHABLEND);
		border_rect.right = 1;
		border_rect.bottom = rect.bottom;
		border_rect_pos.y = pos.y;
		background->Draw(texture, &border_rect, NULL, &border_rect_pos, border_color);
		background->End();

		// Right
		background->Begin(D3DXSPRITE_ALPHABLEND);
		border_rect_pos.x += rect.right;
		background->Draw(texture, &border_rect, NULL, &border_rect_pos, border_color);
		background->End();
	}
}

void DrawGridText(char text[], RECT pos, int align, D3DCOLOR text_color)
{
	grid_Font->DrawText(NULL, text, -1, &pos, align, text_color);
}

D3DCOLOR GetBoarderColor(D3DXVECTOR3 box_pos, RECT box_rect)
{
	const D3DCOLOR BORDER_COLOR = 0x88FFFFFF;
	const D3DCOLOR EDIT_MODE_BORDER_COLOR = 0xFF5555AA;
	const D3DCOLOR MOUSE_HOVER_BORDER_COLOR = 0xFF8888FF;

	POINT mouse_pos;
	if (in_edit_mode && GetCursorPos(&mouse_pos))
	{
		if (MouseIsInBox(mouse_pos, box_pos, box_rect))
			return MOUSE_HOVER_BORDER_COLOR;

		return EDIT_MODE_BORDER_COLOR;
	}
	return BORDER_COLOR;
}

int GetTopGridPosition(int playerArrayPos)
{
	if ((int)drivers.size() <= 7)
		return 0;

	if (playerArrayPos < 4)
		return 0;

	if (playerArrayPos >= (int)drivers.size() - 3)
		return (int)drivers.size() - 7;

	return playerArrayPos - 3;
}

D3DCOLOR rF2_Relative::GetGridTextColor(int other, int player)
{
	bool isPlayer = drivers[other].control == 0 || drivers[other].isPlayer;

	// if it's us, we want player color
	if (isPlayer)
	{
		if (drivers[other].inPits)
			return PlayerInPitsColor;
		return PlayerOnTrackColor;
	}

	if (in_race)
	{
		// if a faster car is >.5 lap ahead of us, we want faster car color
		if ((drivers[other].place < drivers[player].place && drivers[other].relativeTime > 0) ||
			(drivers[other].place < drivers[player].place && drivers[other].totalLapDistance > drivers[player].totalLapDistance + 1))
		{
			if (drivers[other].inPits)
				return FasterCarInPitsColor;
			return FasterCarOnTrackColor;
		}

		// else if a slower car is >.5 lap behind us, we want slower car color
		else if ((drivers[other].place > drivers[player].place && drivers[other].relativeTime < 0) ||
			(drivers[other].place > drivers[player].place && drivers[other].totalLapDistance < drivers[player].totalLapDistance - 1))
		{
			if (drivers[other].inPits)
				return SlowerCarInPitsColor;
			return SlowerCarOnTrackColor;
		}

		// else we fight for position and want equal car color
		else
		{
			if (drivers[other].inPits)
				return EqualCarInPitsColor;
			return EqualCarOnTrackColor;
		}




		//	// else if a faster car is >.5 lap ahead of us, we want red
		//	if (drivers[other].totalLaps + (drivers[other].lapDistance / drivers[other].fullLapDistance) >
		//		drivers[player].totalLaps + (drivers[player].lapDistance / drivers[player].fullLapDistance) + 0.5)
		//	{
		//		if (drivers[other].inPits)
		//			return FasterCarInPitsColor;
		//		return FasterCarOnTrackColor;
		//	}

		//	// else if a slower car is >.5 lap behind us, we want blue
		//	else if (drivers[other].totalLaps + (drivers[other].lapDistance / drivers[other].fullLapDistance) <
		//		drivers[player].totalLaps + (drivers[player].lapDistance / drivers[player].fullLapDistance) - 0.5)
		//	{
		//		if (drivers[other].inPits)
		//			return SlowerCarInPitsColor;
		//		return SlowerCarOnTrackColor;
		//	}

		//	// else we fight for position and want white
		//	else
		//	{
		//		if (drivers[other].inPits)
		//			return EqualCarInPitsColor;
		//		return EqualCarOnTrackColor;
		//	}
		//}


	}
	// if not in race we want white
	else
	{
		if (drivers[other].inPits)
			return EqualCarInPitsColor;
		return EqualCarOnTrackColor;
	}
}

D3DCOLOR GetClassColor(int color)
{
	return CLASS_COLORS[color];
}

void rF2_Relative::DrawGridWidget(D3DCOLOR text_color, D3DCOLOR shadow_color)
{
	if (!grid.enabled)
		return;

	// Box
	grid.rect = { 0, 0, 350, 168 };
	grid.pos.z = 0;

	grid.rect.right *= grid.sizeMultiplier;
	grid.rect.bottom *= grid.sizeMultiplier;


	// Move widget
	POINT mouse_pos;
	if (in_edit_mode && GetCursorPos(&mouse_pos))
	{
		if (MouseIsInBox(mouse_pos, grid.pos, grid.rect))
		{
			if ((GetKeyState(VK_LBUTTON) & 0x100) != 0)
			{
				if (MouseLockedTo == MouseLocked::None || MouseLockedTo == MouseLocked::Grid)
				{
					MouseLockedTo = MouseLocked::Grid;
					grid.pos.x = mouse_pos.x - grid.rect.right / 2;
					grid.pos.y = mouse_pos.y - grid.rect.bottom / 2;
					grid.offset.x = grid.pos.x - resolution.x / 2;
					grid.offset.y = grid.pos.y - resolution.y / 2;
				}
			}
		}
	}

	D3DCOLOR border_color = GetBoarderColor(grid.pos, grid.rect);
	DrawBox(border_color, grid.pos, grid.rect);


	// Text
	RECT placePosition, classBoxRect, placeInClassPosition, namePosition, timePosition;

	placePosition.left = grid.pos.x + 11 * grid.sizeMultiplier;
	placePosition.top = grid.pos.y + 6 * grid.sizeMultiplier;
	placePosition.right = placePosition.left + 20 * grid.sizeMultiplier;
	placePosition.bottom = placePosition.top + 20 * grid.sizeMultiplier;

	classBoxRect.left = placePosition.left + 28 * grid.sizeMultiplier;
	classBoxRect.top = placePosition.top;
	classBoxRect.right = classBoxRect.left + 5 * grid.sizeMultiplier;
	classBoxRect.bottom = classBoxRect.top + 19 * grid.sizeMultiplier;

	placeInClassPosition.left = classBoxRect.left + 10 * grid.sizeMultiplier;
	placeInClassPosition.top = placePosition.top;
	placeInClassPosition.right = placeInClassPosition.left + 20 * grid.sizeMultiplier;
	placeInClassPosition.bottom = placeInClassPosition.top + 20 * grid.sizeMultiplier;

	namePosition.left = placeInClassPosition.left + 26 * grid.sizeMultiplier;
	namePosition.top = placePosition.top;
	namePosition.right = namePosition.left + 220 * grid.sizeMultiplier;
	namePosition.bottom = namePosition.top + 20 * grid.sizeMultiplier;

	timePosition.left = namePosition.left + 220 * grid.sizeMultiplier;
	timePosition.top = placePosition.top;
	timePosition.right = timePosition.left + 47 * grid.sizeMultiplier;
	timePosition.bottom = timePosition.top + 20 * grid.sizeMultiplier;

	if (!is_multi_class)
		namePosition.left -= (30 * grid.sizeMultiplier);

	if (grid.mode == 2)
		classBoxRect.right = classBoxRect.left + 32 * grid.sizeMultiplier;
		//classBoxRect.right += (27 * grid.sizeMultiplier);


	D3DXVECTOR3 classBoxPos = { 0,0,0 };
	classBoxPos.x = classBoxRect.left;
	classBoxPos.y = classBoxRect.top;


	int playerArrayPos = 0;
	for (int i = 0; i < (int)drivers.size(); i++)
	{
		if (!(drivers[i].control == 0 || drivers[i].isPlayer))
			continue;
		playerArrayPos = i;
	}

	int gridStartNum = GetTopGridPosition(playerArrayPos);

	for (int i = 0; i < 7; i++)
	{
		if (gridStartNum + i >= (int)drivers.size())
			break;

		text_color = GetGridTextColor(gridStartNum + i, playerArrayPos);
		bool isPlayer = drivers[gridStartNum + i].control == 0 || drivers[gridStartNum + i].isPlayer;

		char c_buffer[32] = "";
		sprintf(c_buffer, "%d", drivers[gridStartNum + i].place);
		grid_Font->DrawText(NULL, c_buffer, -1, &placePosition, DT_RIGHT, text_color);

		if (is_multi_class)
		{
			D3DCOLOR class_color = 0xFFFFFFFF;
			string currVehicleClass = drivers[gridStartNum + i].vehicleClass;

			class_color = CLASS_COLORS[drivers[gridStartNum + i].classColor];

			if (std::find(userClassColorKeys.begin(), userClassColorKeys.end(), drivers[gridStartNum + i].vehicleClass) != userClassColorKeys.end())
			{
				for (int i = 0; i < userClassColorKeys.size(); i++)
				{
					if (userClassColorKeys[i] == currVehicleClass)
					{
						class_color = userClassColorValues[i];
						break;
					}
				}
			}

			background->Begin(D3DXSPRITE_ALPHABLEND);
			background->Draw(texture, &classBoxRect, NULL, &classBoxPos, class_color);
			background->End();

			sprintf(c_buffer, "%d", drivers[gridStartNum + i].placeInClass);
			D3DCOLOR picColor;
			switch (grid.mode)
			{
			case 1: picColor = text_color;
				break;
			case 2: picColor = 0xFF000000;
				break;
			default: picColor = text_color;
				break;
			}
			grid_Font->DrawText(NULL, c_buffer, -1, &placeInClassPosition, DT_RIGHT, picColor);
		}

		grid_Font->DrawText(NULL, drivers[gridStartNum + i].driverName, -1, &namePosition, DT_LEFT, text_color);

		if (isPlayer)
			sprintf(c_buffer, "%s", "0.0");
		else
			sprintf(c_buffer, "%.1f", drivers[gridStartNum + i].relativeTime * -1);
		grid_Font->DrawText(NULL, c_buffer, -1, &timePosition, DT_RIGHT, text_color);

		//// Adjust for next itteration
		placePosition.top += 23 * grid.sizeMultiplier;
		placePosition.bottom += 23 * grid.sizeMultiplier;

		classBoxRect.top = placePosition.top;
		classBoxRect.bottom = classBoxRect.top + 19 * grid.sizeMultiplier;
		classBoxPos.x = classBoxRect.left;
		classBoxPos.y = classBoxRect.top;

		placeInClassPosition.top = placePosition.top;
		placeInClassPosition.bottom = placeInClassPosition.top + 23 * grid.sizeMultiplier;

		namePosition.top = placePosition.top;
		namePosition.bottom = namePosition.top + 23 * grid.sizeMultiplier;

		timePosition.top = placePosition.top;
		timePosition.bottom = timePosition.top + 23 * grid.sizeMultiplier;
	}
}

void DrawDebug()
{
	if (!devmode)
		return;

	RECT rect = { 10,175,100,300 };
	char c_buffer[32] = "";
	int player = GetPlayerArrayPos();

	sprintf(c_buffer, "Control: %d", drivers[player].control);
	grid_Font->DrawText(NULL, c_buffer, -1, &rect, DT_LEFT, 0xFFFF00FF);
}

void DrawMouseCursor()
{
	if (!in_edit_mode)
		return;

	POINT mouse_pos;
	if (GetCursorPos(&mouse_pos))
	{
		D3DCOLOR color = 0xFFFFFFFF;
		RECT rect = { 0, 0, 32, 32 };
		D3DXVECTOR3 pos = { 0, 0, 0, };
		pos.x = mouse_pos.x;
		pos.y = mouse_pos.y;
		background->Begin(D3DXSPRITE_ALPHABLEND);
		background->Draw(mouse_texture, &rect, NULL, &pos, color);
		background->End();
	}
}

bool rF2_Relative::WantsToDisplayMessage(MessageInfoV01 &msgInfo)
{
	/* Wait until we're in realtime, otherwise
	the message is lost in space */
	if (!in_realtime)
		return false;

	/* We just want to display this message once in this rF2 session */
	if (displayed_welcome)
		return false;

	/* Tell how to toggle display through keyboard */
	msgInfo.mDestination = 0;
	msgInfo.mTranslate = 0;
	sprintf(msgInfo.mText, "Relative Time " PLUGIN_VERSION " enabled (CTRL + E to toggle edit mode)");

	/* Don't do it anymore, just once per session */
	displayed_welcome = true;

	return true;
}

void rF2_Relative::DrawGraphics(const ScreenInfoV01 &info)
{
	LPDIRECT3DDEVICE9 d3d = (LPDIRECT3DDEVICE9)info.mDevice;

	const float SCREEN_WIDTH = info.mWidth;
	const float SCREEN_HEIGHT = info.mHeight;
	const float ASSUMED_SINGLE_SCREEN_WIDTH = SCREEN_HEIGHT / 0.5625;
	const float SCREEN_CENTER = SCREEN_WIDTH / 2.0;

	D3DCOLOR shadow_color = 0xC0000000;
	D3DCOLOR text_color = 0xFFe6e6e6;

	DrawGridWidget(text_color, shadow_color);
	DrawDebug();
	DrawMouseCursor();

	if (!(GetKeyState(VK_LBUTTON) & 0x100) != 0)
		MouseLockedTo = None;
}

void rF2_Relative::RenderScreenAfterOverlays(const ScreenInfoV01 &info)
{
	return;
}

void rF2_Relative::RenderScreenBeforeOverlays(const ScreenInfoV01 &info)
{
	/* If we're not in realtime, not in green flag, etc...
	there's no need to display the fuel widget */
	if (!NeedToDisplay())
		return;

	/* Can't draw without a font object */
	if (grid_Font == NULL)
		return;

	DrawGraphics(info);
}

void rF2_Relative::PreReset(const ScreenInfoV01 &info)
{
	if (big_Font)
		big_Font->OnLostDevice();
	if (small_Font)
		small_Font->OnLostDevice();
	if (grid_Font)
		grid_Font->OnLostDevice();
	if (background)
		background->OnLostDevice();
}

void rF2_Relative::PostReset(const ScreenInfoV01 &info)
{
	if (big_Font)
		big_Font->OnResetDevice();
	if (small_Font)
		small_Font->OnResetDevice();
	if (grid_Font)
		grid_Font->OnResetDevice();
	if (background)
		background->OnResetDevice();
}

void rF2_Relative::ReadUserDefinedClassColors()
{
	userClassColorKeys.clear();
	userClassColorValues.clear();
	char * pBigString = new char[1024];

	// Call GetPrivateProfileSection to populate the buffer.
	DWORD dw = GetPrivateProfileSection("ClassColors", pBigString, 1024, CONFIG_FILE);

	// Enumerate each token in string.
	// The buffer looks like this: "a=1234\0b=2345\0c=ABCD\0\0" 
	// Null bytes ("\0") delimit the strings.
	// The whole buffer is terminated by a double null byte ("\0\0").
	for (LPCSTR pToken = pBigString; pToken && *pToken; pToken = NextToken(pToken))
	{
		string str = pToken;
		size_t breakpos = str.find("=");
		string keySubString = str.substr(0, breakpos);
		char key[1024];
		strncpy(key, keySubString.c_str(), sizeof(key));
		D3DCOLOR value = GetPrivateProfileInt("ClassColors", key, PLAYER_ON_TRACK_COLOR, CONFIG_FILE);

		userClassColorKeys.push_back(key);
		userClassColorValues.push_back(value);
	}
}

LPCSTR ConvertBoolToLPCSTR(bool input)
{
	LPCSTR output;
	if (input)
		output = "1";
	else
		output = "0";
	return output;
}

LPCSTR ConvertFloatToLPCSTR(float input)
{
	LPCSTR output;
	std::stringstream s;
	s << input;
	std::string ws = s.str();
	output = ws.c_str();
	return output;
}

void rF2_Relative::SaveConfig(struct GridConfig &grid, const char *ini_file)
{
	char buff[32];

	// [Grid] section
	WritePrivateProfileString("Grid", "Enabled", ConvertBoolToLPCSTR(grid.enabled), ini_file);
	sprintf(buff, " %.0f", grid.offset.x);
	WritePrivateProfileString("Grid", "PosX", buff, ini_file);
	sprintf(buff, " %.0f", grid.offset.y);
	WritePrivateProfileString("Grid", "PosY", buff, ini_file);
}

void rF2_Relative::LoadConfig(struct PluginConfig &config, struct GridConfig &grid, const char *ini_file)
{
	char str[8];

	// [Config] section
	config.use_borders = GetPrivateProfileInt("Config", "Borders", USE_BORDERS, ini_file);
	config.background_color = GetPrivateProfileInt("Config", "BackgroundColor", BACKGROUND_COLOR, ini_file);
	config.big_font_size = GetPrivateProfileInt("Config", "BigFontSize", BIG_FONT_SIZE, ini_file);
	config.small_font_size = GetPrivateProfileInt("Config", "SmallFontSize", SMALL_FONT_SIZE, ini_file);
	GetPrivateProfileString("Config", "BigFontName", BIG_FONT_NAME, config.big_font_name, FONT_NAME_MAXLEN, ini_file);
	GetPrivateProfileString("Config", "SmallFontName", SMALL_FONT_NAME, config.small_font_name, FONT_NAME_MAXLEN, ini_file);

	// [Input] section
	config.editkey = GetPrivateProfileInt("Input", "EditKey", DEFAULT_EDIT_KEY, ini_file);

	// [Grid] section
	grid.enabled = GetPrivateProfileInt("Grid", "Enabled", DEFAULT_WIDGET_ENABLED, ini_file);
	GetPrivateProfileString("Grid", "Font", GRID_FONT_NAME, config.grid_font_name, FONT_NAME_MAXLEN, ini_file);
	config.grid_font_size = GetPrivateProfileInt("Grid", "FontSize", GRID_FONT_SIZE, ini_file);
	grid.mode = GetPrivateProfileInt("Grid", "Mode", 1, ini_file);
	GetPrivateProfileString("Grid", "PosX", DEFAULT_WIDGET_POSITION, str, 8, ini_file);
	grid.offset.x = std::stoi(str);
	GetPrivateProfileString("Grid", "PosY", DEFAULT_WIDGET_POSITION, str, 8, ini_file);
	grid.offset.y = std::stoi(str);
	GetPrivateProfileString("Grid", "SizeMultiplier", "1.0", str, 8, ini_file);
	grid.sizeMultiplier = std::stod(str);
	PlayerOnTrackColor = GetPrivateProfileInt("Grid", "PlayerOnTrackColor", PLAYER_ON_TRACK_COLOR, ini_file);
	PlayerInPitsColor = GetPrivateProfileInt("Grid", "PlayerInPitsColor", PLAYER_IN_PITS_COLOR, ini_file);
	EqualCarOnTrackColor = GetPrivateProfileInt("Grid", "EqualCarOnTrackColor", EQUAL_ON_TRACK_COLOR, ini_file);
	EqualCarInPitsColor = GetPrivateProfileInt("Grid", "EqualCarInPitsColor", EQUAL_IN_PITS_COLOR, ini_file);
	FasterCarOnTrackColor = GetPrivateProfileInt("Grid", "FasterCarOnTrackColor", FASTER_ON_TRACK_COLOR, ini_file);
	FasterCarInPitsColor = GetPrivateProfileInt("Grid", "FasterCarInPitsColor", FASTER_IN_PITS_COLOR, ini_file);
	SlowerCarOnTrackColor = GetPrivateProfileInt("Grid", "SlowerCarOnTrackColor", SLOWER_ON_TRACK_COLOR, ini_file);
	SlowerCarInPitsColor = GetPrivateProfileInt("Grid", "SlowerCarInPitsColor", SLOWER_IN_PITS_COLOR, ini_file);
}