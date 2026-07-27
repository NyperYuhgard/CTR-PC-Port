#include <common.h>
#include <platform/native_mods.h>
#include <platform/native_config.h>
#include <platform/native_input.h>

// NOTE: buttonTapPerPlayer index access
#define BTN_TAP_PLAYER0 sdata->buttonTapPerPlayer[0]

#define OPTIONS_MENU_VISIBLE_ROWS 8
#define OPTIONS_MENU_CENTER_X     0x100
#define OPTIONS_MENU_PANEL_W      0x1C0
#define OPTIONS_MENU_TITLE_Y      0x1C
#define OPTIONS_MENU_LIST_TOP_Y   0x3C
#define OPTIONS_MENU_ROW_HEIGHT   0x10
#define OPTIONS_MENU_LEFT_X       (OPTIONS_MENU_CENTER_X - OPTIONS_MENU_PANEL_W / 2)
#define OPTIONS_MENU_RIGHT_X      (OPTIONS_MENU_CENTER_X + OPTIONS_MENU_PANEL_W / 2)
#define OPTIONS_MENU_NAME_X       (OPTIONS_MENU_LEFT_X + 0x10)
#define OPTIONS_MENU_VALUE_X      (OPTIONS_MENU_RIGHT_X - 0x60)
#define OPTIONS_MENU_HIGHLIGHT_W  (OPTIONS_MENU_PANEL_W - 0x20)

#define OPTION_ROW_FX        0
#define OPTION_ROW_MUSIC     1
#define OPTION_ROW_VOICE     2
#define OPTION_ROW_MODE      3
#define OPTION_ROW_ASPECT    4
#define OPTION_ROW_FPS       5
#define OPTION_ROW_FILTER    6
#define OPTION_ROW_SCALE     7
#define OPTION_ROW_FULLSCREEN 8
#define OPTION_ROW_CONTROLS  9
#define OPTION_ROW_GAMETWEAKS 10
#ifdef CTR_NATIVE_DEV_HUD_EDITOR
#define OPTION_ROW_DEVTOOLS  11
#define OPTION_ROW_COUNT     12
#else
#define OPTION_ROW_COUNT     11
#endif

#define ASPECT_4_3     0
#define ASPECT_16_9    1
#define ASPECT_STRETCH 2
#define ASPECT_16_9_WS 3

#define OPTIONS_STATE_MAIN          0
#define OPTIONS_STATE_CONTROLS      1
#define OPTIONS_STATE_DEVICELIST    2
#define OPTIONS_STATE_KEYS          3
#define OPTIONS_STATE_WAITING       4
#define OPTIONS_STATE_GAMEPAD       5
#define OPTIONS_STATE_WAITING_GAMEPAD 6

#define CONTROLS_ROW_DEVICE   0
#define CONTROLS_ROW_KEYBIND  1
#define CONTROLS_ROW_GPBIND   2
#define CONTROLS_ROW_COUNT    3

static int s_optionsSelectedIndex = 0;
static int s_optionsState = OPTIONS_STATE_MAIN;
static int s_optionsScrollOffset = 0;
static int s_optionsBindScrollOffset = 0;
static int s_optionsBindSelectedIndex = 0;
static int s_optionsWaitingForKey = 0;
static int s_optionsPrevKeyboardState[576]; // SDL_SCANCODE_COUNT max
static int s_optionsPrevKeyboardStateCount;
static int s_controlsPlayerIndex = 0;
static int s_deviceListIndex = 0;

extern int g_cfg_bilinearFiltering;
extern int g_cfg_aspectMode;
extern int g_cfg_fullscreen;
extern int g_cfg_resolutionScale;
extern void NativeRenderer_UpdateSwapIntervalState(int swapInterval);
extern void NativeRenderer_SetAspectMode(int mode);
extern void NativeRenderer_SetResolutionScale(int scale);
extern void Platform_ToggleFullscreen(void);

static char *Options_GetKeyDisplayName(int scancode)
{
	static char buf[32];
	const char *name = Platform_InputGetScancodeName(scancode);
	if (name != NULL && name[0] != '\0')
	{
		snprintf(buf, sizeof(buf), "%s", name);
	}
	else
	{
		snprintf(buf, sizeof(buf), "Key %d", scancode);
	}
	return buf;
}

static void Options_ResetPrevKeyboardState(void)
{
	int scancodeCount = Platform_InputGetScancodeCount();
	int i;

	s_optionsPrevKeyboardStateCount = scancodeCount;
	if (s_optionsPrevKeyboardStateCount > 576)
		s_optionsPrevKeyboardStateCount = 576;

	for (i = 0; i < s_optionsPrevKeyboardStateCount; i++)
		s_optionsPrevKeyboardState[i] = 0;
}

void MM_Options_Init(void)
{
	s_optionsSelectedIndex = 0;
	s_optionsState = OPTIONS_STATE_MAIN;
	s_optionsScrollOffset = 0;
	s_optionsBindScrollOffset = 0;
	s_optionsBindSelectedIndex = 0;
	s_optionsWaitingForKey = 0;
	s_controlsPlayerIndex = 0;
	s_deviceListIndex = 0;
	Options_ResetPrevKeyboardState();
}

static void Options_SaveConfig(void)
{
	NativeConfig_Save();
}

static void MM_Options_HandleMainInput(void)
{
	if (BTN_TAP_PLAYER0 & BTN_UP)
	{
		if (s_optionsSelectedIndex > 0)
			s_optionsSelectedIndex--;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & BTN_DOWN)
	{
		if (s_optionsSelectedIndex < OPTION_ROW_COUNT - 1)
			s_optionsSelectedIndex++;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & BTN_R2)
	{
		if (s_optionsSelectedIndex + OPTIONS_MENU_VISIBLE_ROWS < OPTION_ROW_COUNT)
			s_optionsSelectedIndex += OPTIONS_MENU_VISIBLE_ROWS;
		else
			s_optionsSelectedIndex = OPTION_ROW_COUNT - 1;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & BTN_L2)
	{
		if (s_optionsSelectedIndex >= OPTIONS_MENU_VISIBLE_ROWS)
			s_optionsSelectedIndex -= OPTIONS_MENU_VISIBLE_ROWS;
		else
			s_optionsSelectedIndex = 0;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}

	if (sdata->AnyPlayerHold & (BTN_LEFT | BTN_RIGHT))
	{
		int delta = 0;
		if (sdata->AnyPlayerHold & BTN_LEFT) delta = -8;
		else if (sdata->AnyPlayerHold & BTN_RIGHT) delta = 8;

		if (s_optionsSelectedIndex == OPTION_ROW_FX)
		{
			int v = (howl_VolumeGet(0) & 0xff) + delta;
			if (v < 0) v = 0;
			if (v > 255) v = 255;
			howl_VolumeSet(0, (u8)v);
			RECTMENU_ClearInput();
			Options_SaveConfig();
		}
		else if (s_optionsSelectedIndex == OPTION_ROW_MUSIC)
		{
			int v = (howl_VolumeGet(1) & 0xff) + delta;
			if (v < 0) v = 0;
			if (v > 255) v = 255;
			howl_VolumeSet(1, (u8)v);
			RECTMENU_ClearInput();
			Options_SaveConfig();
		}
		else if (s_optionsSelectedIndex == OPTION_ROW_VOICE)
		{
			int v = (howl_VolumeGet(2) & 0xff) + delta;
			if (v < 0) v = 0;
			if (v > 255) v = 255;
			howl_VolumeSet(2, (u8)v);
			RECTMENU_ClearInput();
			Options_SaveConfig();
		}
	}

	if (BTN_TAP_PLAYER0 & BTN_CROSS)
	{
		OtherFX_Play(1, 1);
		RECTMENU_ClearInput();

		if (s_optionsSelectedIndex == OPTION_ROW_MODE)
		{
			int mode = howl_ModeGet();
			howl_ModeSet(!mode);
			Options_SaveConfig();
		}
		else if (s_optionsSelectedIndex == OPTION_ROW_ASPECT)
		{
			g_cfg_aspectMode = (g_cfg_aspectMode + 1) % 4;
			NativeRenderer_SetAspectMode(g_cfg_aspectMode);
			Options_SaveConfig();
		}
		else if (s_optionsSelectedIndex == OPTION_ROW_FPS)
		{
			g_cfg_60fpsMode = !g_cfg_60fpsMode;
			NativeRenderer_UpdateSwapIntervalState(g_cfg_60fpsMode != 0 ? 1 : -1);
			Options_SaveConfig();
		}
		else if (s_optionsSelectedIndex == OPTION_ROW_SCALE)
		{
			int scales[] = {1, 2, 3, 4};
			int dir = (sdata->AnyPlayerHold & BTN_RIGHT) ? 1 : -1;
			int idx = 0;
			for (int i = 0; i < 4; i++)
			{
				if (g_cfg_resolutionScale == scales[i])
				{
					idx = (i + dir + 4) % 4;
					break;
				}
			}
			NativeRenderer_SetResolutionScale(scales[idx]);
			Options_SaveConfig();
		}
		else if (s_optionsSelectedIndex == OPTION_ROW_FILTER)
		{
			g_cfg_bilinearFiltering = !g_cfg_bilinearFiltering;
			Options_SaveConfig();
		}
		else if (s_optionsSelectedIndex == OPTION_ROW_FULLSCREEN)
		{
			g_cfg_fullscreen = !g_cfg_fullscreen;
			Platform_ToggleFullscreen();
			Options_SaveConfig();
		}
		else if (s_optionsSelectedIndex == OPTION_ROW_CONTROLS)
		{
			s_controlsPlayerIndex = 0;
			s_optionsSelectedIndex = 0;
			s_optionsState = OPTIONS_STATE_CONTROLS;
		}
		else if (s_optionsSelectedIndex == OPTION_ROW_GAMETWEAKS)
		{
			MM_GameplayTweaks_Init();
			RECTMENU_ClearInput();
			sdata->ptrDesiredMenu = MM_GameplayTweaks_GetMenuPtr();
		}
#ifdef CTR_NATIVE_DEV_HUD_EDITOR
		else if (s_optionsSelectedIndex == OPTION_ROW_DEVTOOLS)
		{
			MM_DevTools_Init();
			RECTMENU_ClearInput();
			sdata->ptrDesiredMenu = MM_DevTools_GetMenuPtr();
		}
#endif
	}

	if (BTN_TAP_PLAYER0 & (BTN_TRIANGLE | BTN_SQUARE))
	{
		OtherFX_Play(2, 1);
		RECTMENU_ClearInput();
		MM_JumpTo_Title_Returning();
	}
}

static void Options_ClampScroll(void)
{
	if (OPTION_ROW_COUNT <= OPTIONS_MENU_VISIBLE_ROWS)
	{
		s_optionsScrollOffset = 0;
		return;
	}

	int maxScroll = OPTION_ROW_COUNT - OPTIONS_MENU_VISIBLE_ROWS;

	if (s_optionsSelectedIndex < s_optionsScrollOffset)
		s_optionsScrollOffset = s_optionsSelectedIndex;
	else if (s_optionsSelectedIndex >= s_optionsScrollOffset + OPTIONS_MENU_VISIBLE_ROWS)
		s_optionsScrollOffset = s_optionsSelectedIndex - OPTIONS_MENU_VISIBLE_ROWS + 1;

	if (s_optionsScrollOffset < 0)
		s_optionsScrollOffset = 0;
	if (s_optionsScrollOffset > maxScroll)
		s_optionsScrollOffset = maxScroll;
}

static void Options_DrawMain(struct GameTracker *gGT, u_long *ot)
{
	int i;
	int y = OPTIONS_MENU_LIST_TOP_Y;
	int selected = s_optionsSelectedIndex;
	int visibleCount;
	RECT highlight;
	RECT borders;

	Options_ClampScroll();

	// Highlight bar
	if (selected >= s_optionsScrollOffset && selected < s_optionsScrollOffset + OPTIONS_MENU_VISIBLE_ROWS)
	{
		int relIndex = selected - s_optionsScrollOffset;
		highlight.x = OPTIONS_MENU_LEFT_X + 4;
		highlight.y = OPTIONS_MENU_LIST_TOP_Y + (relIndex * OPTIONS_MENU_ROW_HEIGHT) - 1;
		highlight.w = OPTIONS_MENU_HIGHLIGHT_W;
		highlight.h = OPTIONS_MENU_ROW_HEIGHT + 1;
		CTR_Box_DrawClearBox(&highlight, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
	}

	// Draw visible rows
	visibleCount = OPTION_ROW_COUNT - s_optionsScrollOffset;
	if (visibleCount > OPTIONS_MENU_VISIBLE_ROWS)
		visibleCount = OPTIONS_MENU_VISIBLE_ROWS;

	for (i = 0; i < visibleCount; i++)
	{
		int absIdx = s_optionsScrollOffset + i;

		switch (absIdx)
		{
			case OPTION_ROW_FX:
			{
				int vol = howl_VolumeGet(0) & 0xff;
				DecalFont_DrawLineOT(sdata->lngStrings[LNG_FX], OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
				char buf[8];
				snprintf(buf, sizeof(buf), "%d", vol);
				DecalFont_DrawLineOT(buf, OPTIONS_MENU_VALUE_X, y, FONT_SMALL, WHITE, ot);
				break;
			}

			case OPTION_ROW_MUSIC:
			{
				int vol = howl_VolumeGet(1) & 0xff;
				DecalFont_DrawLineOT(sdata->lngStrings[LNG_MUSIC], OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
				char buf[8];
				snprintf(buf, sizeof(buf), "%d", vol);
				DecalFont_DrawLineOT(buf, OPTIONS_MENU_VALUE_X, y, FONT_SMALL, WHITE, ot);
				break;
			}

			case OPTION_ROW_VOICE:
			{
				int vol = howl_VolumeGet(2) & 0xff;
				DecalFont_DrawLineOT(sdata->lngStrings[LNG_VOICE], OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
				char buf[8];
				snprintf(buf, sizeof(buf), "%d", vol);
				DecalFont_DrawLineOT(buf, OPTIONS_MENU_VALUE_X, y, FONT_SMALL, WHITE, ot);
				break;
			}

			case OPTION_ROW_MODE:
			{
				int mode = howl_ModeGet();
				DecalFont_DrawLineOT("Sound Type", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
				DecalFont_DrawLineOT(sdata->lngStrings[mode ? LNG_STEREO : LNG_MONO], OPTIONS_MENU_VALUE_X, y, FONT_SMALL, WHITE, ot);
				break;
			}

			case OPTION_ROW_ASPECT:
			{
				char *label = g_cfg_aspectMode == ASPECT_STRETCH ? "STRETCH" :
					g_cfg_aspectMode == ASPECT_16_9_WS ? "16:9 WS" :
					g_cfg_aspectMode == ASPECT_16_9 ? "16:9" : "4:3";
				int color = g_cfg_aspectMode == ASPECT_4_3 ? WHITE : TINY_GREEN;
				DecalFont_DrawLineOT("ASPECT", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
				DecalFont_DrawLineOT(label, OPTIONS_MENU_VALUE_X, y, FONT_SMALL, color, ot);
				break;
			}

			case OPTION_ROW_FPS:
			{
				char *fpsLabels[] = {"30", "60"};
				int fpsColor = g_cfg_60fpsMode == 1 ? TINY_GREEN : WHITE;
				DecalFont_DrawLineOT("FPS", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
				DecalFont_DrawLineOT(fpsLabels[g_cfg_60fpsMode], OPTIONS_MENU_VALUE_X, y, FONT_SMALL, fpsColor, ot);
				break;
			}

			case OPTION_ROW_FILTER:
			{
				DecalFont_DrawLineOT("FILTER", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
				DecalFont_DrawLineOT(g_cfg_bilinearFiltering ? "SMOOTH" : "PIXEL", OPTIONS_MENU_VALUE_X, y, FONT_SMALL, g_cfg_bilinearFiltering ? TINY_GREEN : WHITE, ot);
				break;
			}

			case OPTION_ROW_SCALE:
			{
				char *scaleLabels[] = {"1x", "2x", "3x", "4x"};
				int scaleColor = g_cfg_resolutionScale > 1 ? TINY_GREEN : WHITE;
				DecalFont_DrawLineOT("SCALE", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
				DecalFont_DrawLineOT(scaleLabels[g_cfg_resolutionScale - 1], OPTIONS_MENU_VALUE_X, y, FONT_SMALL, scaleColor, ot);
				break;
			}

			case OPTION_ROW_FULLSCREEN:
			{
				DecalFont_DrawLineOT("FULLSCREEN", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
				DecalFont_DrawLineOT(g_cfg_fullscreen ? "ON" : "OFF", OPTIONS_MENU_VALUE_X, y, FONT_SMALL, g_cfg_fullscreen ? TINY_GREEN : WHITE, ot);
				break;
			}

			case OPTION_ROW_CONTROLS:
			{
				DecalFont_DrawLineOT("CONTROLS", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
				DecalFont_DrawLineOT("EDIT", OPTIONS_MENU_VALUE_X, y, FONT_SMALL, TINY_GREEN, ot);
				break;
			}

			case OPTION_ROW_GAMETWEAKS:
			{
				int anyOn = g_cfg_specialItems || g_cfg_cpuAllItems ||
						g_cfg_itemChaos || g_cfg_cpuItemChaos || g_cfg_chaosRng;
				DecalFont_DrawLineOT("GAMEPLAY TWEAKS", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
				DecalFont_DrawLineOT(anyOn ? "ON" : ">", OPTIONS_MENU_VALUE_X, y, FONT_SMALL, anyOn ? TINY_GREEN : WHITE, ot);
				break;
			}

#ifdef CTR_NATIVE_DEV_HUD_EDITOR
			case OPTION_ROW_DEVTOOLS:
			{
				DecalFont_DrawLineOT("DEV TOOLS", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
				DecalFont_DrawLineOT(">", OPTIONS_MENU_VALUE_X, y, FONT_SMALL, TINY_GREEN, ot);
				break;
			}
#endif
		}

		y += OPTIONS_MENU_ROW_HEIGHT;
	}

	// Scroll indicators
	if (s_optionsScrollOffset > 0)
	{
		DecalFont_DrawLineOT("^", OPTIONS_MENU_CENTER_X, OPTIONS_MENU_LIST_TOP_Y - 0x0C, FONT_SMALL, JUSTIFY_CENTER | WHITE, ot);
	}
	if (OPTION_ROW_COUNT > s_optionsScrollOffset + OPTIONS_MENU_VISIBLE_ROWS)
	{
		DecalFont_DrawLineOT("v", OPTIONS_MENU_CENTER_X, y + 2, FONT_SMALL, JUSTIFY_CENTER | WHITE, ot);
	}

	// Page indicator
	if (OPTION_ROW_COUNT > OPTIONS_MENU_VISIBLE_ROWS)
	{
		char pageBuf[32];
		snprintf(pageBuf, sizeof(pageBuf), "%d / %d", s_optionsSelectedIndex + 1, OPTION_ROW_COUNT);
		DecalFont_DrawLineOT(pageBuf, OPTIONS_MENU_CENTER_X, y + 0x10, FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);
	}

	int listHeight = OPTIONS_MENU_VISIBLE_ROWS * OPTIONS_MENU_ROW_HEIGHT;
	borders.x = OPTIONS_MENU_CENTER_X - OPTIONS_MENU_PANEL_W / 2 - 6;
	borders.y = OPTIONS_MENU_TITLE_Y - 8;
	borders.w = OPTIONS_MENU_PANEL_W + 12;
	borders.h = listHeight + (OPTIONS_MENU_LIST_TOP_Y - OPTIONS_MENU_TITLE_Y) + 0x30;
	RECTMENU_DrawInnerRect(&borders, 0, ot);
}

static void Options_Keys_ClampScroll(void)
{
	if (PLATFORM_INPUT_BINDING_COUNT <= OPTIONS_MENU_VISIBLE_ROWS)
	{
		s_optionsBindScrollOffset = 0;
		return;
	}

	int maxScroll = PLATFORM_INPUT_BINDING_COUNT - OPTIONS_MENU_VISIBLE_ROWS;

	if (s_optionsBindSelectedIndex < s_optionsBindScrollOffset)
		s_optionsBindScrollOffset = s_optionsBindSelectedIndex;
	else if (s_optionsBindSelectedIndex >= s_optionsBindScrollOffset + OPTIONS_MENU_VISIBLE_ROWS)
		s_optionsBindScrollOffset = s_optionsBindSelectedIndex - OPTIONS_MENU_VISIBLE_ROWS + 1;

	if (s_optionsBindScrollOffset < 0)
		s_optionsBindScrollOffset = 0;
	if (s_optionsBindScrollOffset > maxScroll)
		s_optionsBindScrollOffset = maxScroll;
}

static void Options_DrawKeys(u_long *ot)
{
	int i;
	int y = OPTIONS_MENU_LIST_TOP_Y;
	int visibleCount;
	RECT highlight;
	RECT borders;

	Options_Keys_ClampScroll();

	// Player tab
	{
		char tabBuf[16];
		snprintf(tabBuf, sizeof(tabBuf), "P%d KEYBOARD", s_controlsPlayerIndex + 1);
		DecalFont_DrawLineOT(tabBuf, OPTIONS_MENU_CENTER_X, OPTIONS_MENU_TITLE_Y + 0x10, FONT_SMALL, JUSTIFY_CENTER | TINY_GREEN, ot);
	}

	// Highlight bar
	if (PLATFORM_INPUT_BINDING_COUNT > 0)
	{
		int relIndex = s_optionsBindSelectedIndex - s_optionsBindScrollOffset;
		highlight.x = OPTIONS_MENU_LEFT_X + 4;
		highlight.y = OPTIONS_MENU_LIST_TOP_Y + (relIndex * OPTIONS_MENU_ROW_HEIGHT) - 1;
		highlight.w = OPTIONS_MENU_HIGHLIGHT_W;
		highlight.h = OPTIONS_MENU_ROW_HEIGHT + 1;
		CTR_Box_DrawClearBox(&highlight, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
	}

	// Binding rows
	visibleCount = PLATFORM_INPUT_BINDING_COUNT - s_optionsBindScrollOffset;
	if (visibleCount > OPTIONS_MENU_VISIBLE_ROWS)
		visibleCount = OPTIONS_MENU_VISIBLE_ROWS;

	for (i = 0; i < visibleCount; i++)
	{
		int absIndex = s_optionsBindScrollOffset + i;
		int scancode;
		char label[64];
		char *keyName;

		if (!Platform_InputGetKeyBinding(s_controlsPlayerIndex, absIndex, &scancode))
			break;

		keyName = Options_GetKeyDisplayName(scancode);

		// Format: "SQUARE    X" or "D-PAD UP    UP"
		const char *actionName = Platform_InputGetActionName(absIndex);
		if (actionName == NULL)
			actionName = "?";

		snprintf(label, sizeof(label), "%s", actionName);
		DecalFont_DrawLineOT(label, OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
		DecalFont_DrawLineOT(keyName, OPTIONS_MENU_VALUE_X, y, FONT_SMALL, WHITE, ot);
		y += OPTIONS_MENU_ROW_HEIGHT;
	}

	// Scroll indicators
	if (s_optionsBindScrollOffset > 0)
	{
		DecalFont_DrawLineOT("^", OPTIONS_MENU_CENTER_X, OPTIONS_MENU_LIST_TOP_Y - 0x0C, FONT_SMALL, JUSTIFY_CENTER | WHITE, ot);
	}
	if (PLATFORM_INPUT_BINDING_COUNT > s_optionsBindScrollOffset + OPTIONS_MENU_VISIBLE_ROWS)
	{
		DecalFont_DrawLineOT("v", OPTIONS_MENU_CENTER_X, y + 2, FONT_SMALL, JUSTIFY_CENTER | WHITE, ot);
	}

	// Page indicator
	if (PLATFORM_INPUT_BINDING_COUNT > OPTIONS_MENU_VISIBLE_ROWS)
	{
		char pageBuf[32];
		snprintf(pageBuf, sizeof(pageBuf), "%d / %d", s_optionsBindSelectedIndex + 1, PLATFORM_INPUT_BINDING_COUNT);
		DecalFont_DrawLineOT(pageBuf, OPTIONS_MENU_CENTER_X, y + 0x10, FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);
	}

	// Help bar
	if (s_optionsWaitingForKey)
	{
		int actionScancode;
		const char *actionName = Platform_InputGetActionName(s_optionsBindSelectedIndex);
		if (actionName == NULL) actionName = "?";
		Platform_InputGetKeyBinding(s_controlsPlayerIndex, s_optionsBindSelectedIndex, &actionScancode);
		char msg[64];
		snprintf(msg, sizeof(msg), "Press key for %s...", actionName);
		DecalFont_DrawLineOT(msg, OPTIONS_MENU_CENTER_X, y + 0x1C, FONT_SMALL, JUSTIFY_CENTER | TINY_GREEN, ot);
		DecalFont_DrawLineOT("ESC to cancel", OPTIONS_MENU_CENTER_X, y + 0x28, FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);
	}
	else
	{
		DecalFont_DrawLineOT("X: Rebind   TRI: Back", OPTIONS_MENU_CENTER_X, y + 0x1C, FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
	}

	// Border
	int listHeight = OPTIONS_MENU_VISIBLE_ROWS * OPTIONS_MENU_ROW_HEIGHT;
	borders.x = OPTIONS_MENU_CENTER_X - OPTIONS_MENU_PANEL_W / 2 - 6;
	borders.y = OPTIONS_MENU_TITLE_Y - 8;
	borders.w = OPTIONS_MENU_PANEL_W + 12;
	borders.h = listHeight + (OPTIONS_MENU_LIST_TOP_Y - OPTIONS_MENU_TITLE_Y) + 0x44;
	RECTMENU_DrawInnerRect(&borders, 0, ot);
}

static void Options_ClearButtonTaps(void)
{
	BTN_TAP_PLAYER0 = 0;
	sdata->AnyPlayerHold = 0;
}

static void Options_HandleKeysInput(void)
{
	int scancodeCount = Platform_InputGetScancodeCount();

	if (s_optionsWaitingForKey)
	{
		int i;

		// Look for a newly pressed key
		for (i = 0; i < scancodeCount; i++)
		{
			if (i < s_optionsPrevKeyboardStateCount && Platform_InputIsKeyDown(i) && !s_optionsPrevKeyboardState[i])
			{
				// Esc cancels (SDL_SCANCODE_ESCAPE = 41)
				if (i == 41)
				{
					s_optionsWaitingForKey = 0;
					Options_ClearButtonTaps();
					RECTMENU_ClearInput();
					return;
				}

				// Skip modifier keys that are commonly held
				if (i == 224 || i == 225 || i == 226 || i == 227 || i == 229 || i == 228)
					continue;

				// Assign the key
				Platform_InputSetKeyBinding(s_controlsPlayerIndex, s_optionsBindSelectedIndex, i);
				Options_SaveConfig();
				s_optionsWaitingForKey = 0;
				// Clear button taps so the key we just assigned doesn't trigger a menu action
				Options_ClearButtonTaps();
				OtherFX_Play(1, 1);
				RECTMENU_ClearInput();
				return;
			}
		}

		// Copy current state for next frame
		for (i = 0; i < scancodeCount && i < s_optionsPrevKeyboardStateCount; i++)
			s_optionsPrevKeyboardState[i] = Platform_InputIsKeyDown(i) ? 1 : 0;

		return;
	}

	if (BTN_TAP_PLAYER0 & BTN_UP)
	{
		s_optionsBindSelectedIndex = (s_optionsBindSelectedIndex - 1 + PLATFORM_INPUT_BINDING_COUNT) % PLATFORM_INPUT_BINDING_COUNT;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & BTN_DOWN)
	{
		s_optionsBindSelectedIndex = (s_optionsBindSelectedIndex + 1) % PLATFORM_INPUT_BINDING_COUNT;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & BTN_CROSS)
	{
		int i;

		s_optionsWaitingForKey = 1;

		// Save current keyboard state as baseline
		Options_ResetPrevKeyboardState();
		for (i = 0; i < scancodeCount && i < s_optionsPrevKeyboardStateCount; i++)
			s_optionsPrevKeyboardState[i] = Platform_InputIsKeyDown(i) ? 1 : 0;

		OtherFX_Play(1, 1);
		Options_ClearButtonTaps();
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & (BTN_TRIANGLE | BTN_SQUARE))
	{
		s_optionsState = OPTIONS_STATE_CONTROLS;
		OtherFX_Play(2, 1);
		Options_ClearButtonTaps();
		RECTMENU_ClearInput();
	}
}

static char *Options_GetGamepadBindingName(int binding)
{
	static const char *buttonNames[] = {
		"A", "B", "X", "Y",
		"LB", "RB", "Back", "Guide",
		"Start", "LS", "RS", "Up",
		"Down", "Left", "Right", "TP",
		"Misc", "Paddle1", "Paddle2", "Paddle3",
		"Paddle4", "Touchpad"
	};
	static char buf[32];
	int axis = binding & ~(0x4000 | 0x8000);
	int isAxis = (binding & 0x4000) != 0;
	int inverse = (binding & 0x8000) != 0;

	if (binding < 0)
	{
		snprintf(buf, sizeof(buf), "None");
	}
	else if (isAxis)
	{
		const char *axisNames[] = {"LX", "LY", "RX", "RY", "L2", "R2"};
		if (axis >= 0 && axis < 6)
			snprintf(buf, sizeof(buf), "%s%s", inverse ? "-" : "+", axisNames[axis]);
		else
			snprintf(buf, sizeof(buf), "Axis %d", axis);
	}
	else
	{
		if (axis >= 0 && axis < 22)
			snprintf(buf, sizeof(buf), "%s", buttonNames[axis]);
		else
			snprintf(buf, sizeof(buf), "Btn %d", binding);
	}
	return buf;
}

static void Options_DrawGamepad(u_long *ot)
{
	int i;
	int y = OPTIONS_MENU_LIST_TOP_Y;
	int visibleCount;
	RECT highlight;
	RECT borders;

	Options_Keys_ClampScroll();

	// Player tab
	{
		char tabBuf[16];
		snprintf(tabBuf, sizeof(tabBuf), "P%d GAMEPAD", s_controlsPlayerIndex + 1);
		DecalFont_DrawLineOT(tabBuf, OPTIONS_MENU_CENTER_X, OPTIONS_MENU_TITLE_Y + 0x10, FONT_SMALL, JUSTIFY_CENTER | TINY_GREEN, ot);
	}

	// Highlight bar
	if (PLATFORM_INPUT_BINDING_COUNT > 0)
	{
		int relIndex = s_optionsBindSelectedIndex - s_optionsBindScrollOffset;
		highlight.x = OPTIONS_MENU_LEFT_X + 4;
		highlight.y = OPTIONS_MENU_LIST_TOP_Y + (relIndex * OPTIONS_MENU_ROW_HEIGHT) - 1;
		highlight.w = OPTIONS_MENU_HIGHLIGHT_W;
		highlight.h = OPTIONS_MENU_ROW_HEIGHT + 1;
		CTR_Box_DrawClearBox(&highlight, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
	}

	// Binding rows
	visibleCount = PLATFORM_INPUT_BINDING_COUNT - s_optionsBindScrollOffset;
	if (visibleCount > OPTIONS_MENU_VISIBLE_ROWS)
		visibleCount = OPTIONS_MENU_VISIBLE_ROWS;

	for (i = 0; i < visibleCount; i++)
	{
		int absIndex = s_optionsBindScrollOffset + i;
		int binding;
		char *bindName;
		const char *actionName = Platform_InputGetGamepadActionName(absIndex);
		if (actionName == NULL) actionName = "?";

		if (!Platform_InputGetGamepadBinding(s_controlsPlayerIndex, absIndex, &binding))
			break;

		bindName = Options_GetGamepadBindingName(binding);

		DecalFont_DrawLineOT((char *)actionName, OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
		DecalFont_DrawLineOT(bindName, OPTIONS_MENU_VALUE_X, y, FONT_SMALL, WHITE, ot);
		y += OPTIONS_MENU_ROW_HEIGHT;
	}

	// Scroll indicators
	if (s_optionsBindScrollOffset > 0)
		DecalFont_DrawLineOT("^", OPTIONS_MENU_CENTER_X, OPTIONS_MENU_LIST_TOP_Y - 0x0C, FONT_SMALL, JUSTIFY_CENTER | WHITE, ot);
	if (PLATFORM_INPUT_BINDING_COUNT > s_optionsBindScrollOffset + OPTIONS_MENU_VISIBLE_ROWS)
		DecalFont_DrawLineOT("v", OPTIONS_MENU_CENTER_X, y + 2, FONT_SMALL, JUSTIFY_CENTER | WHITE, ot);

	// Page indicator
	if (PLATFORM_INPUT_BINDING_COUNT > OPTIONS_MENU_VISIBLE_ROWS)
	{
		char pageBuf[32];
		snprintf(pageBuf, sizeof(pageBuf), "%d / %d", s_optionsBindSelectedIndex + 1, PLATFORM_INPUT_BINDING_COUNT);
		DecalFont_DrawLineOT(pageBuf, OPTIONS_MENU_CENTER_X, y + 0x10, FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);
	}

	// Help bar
	if (s_optionsWaitingForKey)
	{
		const char *actionName = Platform_InputGetGamepadActionName(s_optionsBindSelectedIndex);
		if (actionName == NULL) actionName = "?";
		char msg[64];
		snprintf(msg, sizeof(msg), "Press button for %s...", actionName);
		DecalFont_DrawLineOT(msg, OPTIONS_MENU_CENTER_X, y + 0x1C, FONT_SMALL, JUSTIFY_CENTER | TINY_GREEN, ot);
		DecalFont_DrawLineOT("ESC to cancel", OPTIONS_MENU_CENTER_X, y + 0x28, FONT_SMALL, JUSTIFY_CENTER | GRAY, ot);
	}
	else
	{
		DecalFont_DrawLineOT("X: Rebind   TRI: Back", OPTIONS_MENU_CENTER_X, y + 0x1C, FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
	}

	// Border
	int listHeight = OPTIONS_MENU_VISIBLE_ROWS * OPTIONS_MENU_ROW_HEIGHT;
	borders.x = OPTIONS_MENU_CENTER_X - OPTIONS_MENU_PANEL_W / 2 - 6;
	borders.y = OPTIONS_MENU_TITLE_Y - 8;
	borders.w = OPTIONS_MENU_PANEL_W + 12;
	borders.h = listHeight + (OPTIONS_MENU_LIST_TOP_Y - OPTIONS_MENU_TITLE_Y) + 0x44;
	RECTMENU_DrawInnerRect(&borders, 0, ot);
}

static void Options_HandleGamepadInput(void)
{
	int scancodeCount = Platform_InputGetScancodeCount();
	int i;

	if (s_optionsWaitingForKey)
	{
		// ESC cancels
		if (Platform_InputIsKeyDown(SDL_SCANCODE_ESCAPE))
		{
			s_optionsWaitingForKey = 0;
			Options_ClearButtonTaps();
			RECTMENU_ClearInput();
			return;
		}

		// Check gamepad buttons
		for (i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++)
		{
			if (Platform_InputIsGamepadButtonDown(s_controlsPlayerIndex, i))
			{
				Platform_InputSetGamepadBinding(s_controlsPlayerIndex, s_optionsBindSelectedIndex, i);
				Options_SaveConfig();
				s_optionsWaitingForKey = 0;
				Options_ClearButtonTaps();
				OtherFX_Play(1, 1);
				RECTMENU_ClearInput();
				return;
			}
		}

		// Check gamepad axes
		{
			int axisBindings[] = {
				SDL_GAMEPAD_AXIS_LEFTX,
				SDL_GAMEPAD_AXIS_LEFTY,
				SDL_GAMEPAD_AXIS_RIGHTX,
				SDL_GAMEPAD_AXIS_RIGHTY,
				SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
				SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
			};
			for (i = 0; i < 6; i++)
			{
				int dir = Platform_InputIsGamepadAxisActive(s_controlsPlayerIndex, axisBindings[i], 16000);
				if (dir != 0)
				{
					int binding = axisBindings[i] | 0x4000; // NATIVE_INPUT_MAP_FLAG_AXIS
					if (dir < 0)
						binding |= 0x8000; // NATIVE_INPUT_MAP_FLAG_INVERSE
					Platform_InputSetGamepadBinding(s_controlsPlayerIndex, s_optionsBindSelectedIndex, binding);
					Options_SaveConfig();
					s_optionsWaitingForKey = 0;
					Options_ClearButtonTaps();
					OtherFX_Play(1, 1);
					RECTMENU_ClearInput();
					return;
				}
			}
		}

		return;
	}

	if (BTN_TAP_PLAYER0 & BTN_UP)
	{
		s_optionsBindSelectedIndex = (s_optionsBindSelectedIndex - 1 + PLATFORM_INPUT_BINDING_COUNT) % PLATFORM_INPUT_BINDING_COUNT;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & BTN_DOWN)
	{
		s_optionsBindSelectedIndex = (s_optionsBindSelectedIndex + 1) % PLATFORM_INPUT_BINDING_COUNT;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & BTN_CROSS)
	{
		s_optionsWaitingForKey = 1;
		OtherFX_Play(1, 1);
		Options_ClearButtonTaps();
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & (BTN_TRIANGLE | BTN_SQUARE))
	{
		s_optionsState = OPTIONS_STATE_CONTROLS;
		OtherFX_Play(2, 1);
		Options_ClearButtonTaps();
		RECTMENU_ClearInput();
	}
}

static int Options_GetMaxDevice(void)
{
	return 1 + Platform_InputGetGamepadDeviceCount(); // 1=keyboard + N gamepads
}

static int Options_GetDeviceForPlayer(int playerIndex)
{
	int i;
	int count;
	if (Platform_InputGetKeyboardSlot() == playerIndex)
		return 1; // keyboard
	count = Platform_InputGetGamepadDeviceCount();
	for (i = 0; i < count; i++)
	{
		int joyId = Platform_InputGetGamepadDeviceId(i);
		if (joyId >= 0 && Platform_InputGetGamepadPlayer(joyId) == playerIndex)
			return 2 + i;
	}
	return 0; // none
}

static const char *Options_GetDeviceName(int device)
{
	if (device == 0) return "None";
	if (device == 1) return "Keyboard";
	if (device >= 2)
	{
		static char gpName[32];
		int joyId = Platform_InputGetGamepadDeviceId(device - 2);
		const char *name;
		if (joyId < 0) return "Gamepad ?";
		name = Platform_InputGetGamepadDeviceName(joyId);
		if (name != NULL && name[0] != '\0')
			snprintf(gpName, sizeof(gpName), "%.22s", name);
		else
			snprintf(gpName, sizeof(gpName), "Gamepad %d", device - 1);
		return gpName;
	}
	return "None";
}

static void Options_SetDeviceForPlayer(int playerIndex, int device)
{
	int oldDevice = Options_GetDeviceForPlayer(playerIndex);

	if (oldDevice == device)
		return;

	// Clear old assignment
	if (oldDevice == 1)
		Platform_InputSetKeyboardSlot(-1);
	else if (oldDevice >= 2)
	{
		int joyId = Platform_InputGetGamepadDeviceId(oldDevice - 2);
		if (joyId >= 0)
			Platform_InputSetGamepadToPlayer(joyId, -1);
	}

	// Set new assignment
	if (device == 0)
	{
		Platform_InputClearPlayerSlot(playerIndex);
	}
	else if (device == 1)
		Platform_InputSetKeyboardSlot(playerIndex);
	else if (device >= 2)
	{
		int joyId = Platform_InputGetGamepadDeviceId(device - 2);
		if (joyId >= 0)
			Platform_InputSetGamepadToPlayer(joyId, playerIndex);
	}
}

static void Options_DrawControls(u_long *ot)
{
	int y = OPTIONS_MENU_LIST_TOP_Y;
	int selected = s_optionsSelectedIndex;
	RECT highlight;
	RECT borders;
	int device;
	const char *deviceName;
	char playerTab[8];

	// Player tab header
	snprintf(playerTab, sizeof(playerTab), "P%d", s_controlsPlayerIndex + 1);
	DecalFont_DrawLineOT(playerTab, OPTIONS_MENU_CENTER_X, OPTIONS_MENU_TITLE_Y + 0x10, FONT_SMALL, JUSTIFY_CENTER | TINY_GREEN, ot);

	// Highlight bar
	{
		highlight.x = OPTIONS_MENU_LEFT_X + 4;
		highlight.y = OPTIONS_MENU_LIST_TOP_Y + (selected * OPTIONS_MENU_ROW_HEIGHT) - 1;
		highlight.w = OPTIONS_MENU_HIGHLIGHT_W;
		highlight.h = OPTIONS_MENU_ROW_HEIGHT + 1;
		CTR_Box_DrawClearBox(&highlight, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
	}

	// Row 0: Device
	device = Options_GetDeviceForPlayer(s_controlsPlayerIndex);
	deviceName = Options_GetDeviceName(device);
	DecalFont_DrawLineOT("DEVICE", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
	DecalFont_DrawLineOT((char *)deviceName, OPTIONS_MENU_VALUE_X, y, FONT_SMALL, WHITE, ot);
	y += OPTIONS_MENU_ROW_HEIGHT;

	// Row 1: Keyboard rebind
	DecalFont_DrawLineOT("KEYBOARD", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
	DecalFont_DrawLineOT("EDIT", OPTIONS_MENU_VALUE_X, y, FONT_SMALL, TINY_GREEN, ot);
	y += OPTIONS_MENU_ROW_HEIGHT;

	// Row 2: Gamepad rebind
	DecalFont_DrawLineOT("GAMEPAD", OPTIONS_MENU_NAME_X, y, FONT_SMALL, ORANGE, ot);
	DecalFont_DrawLineOT("EDIT", OPTIONS_MENU_VALUE_X, y, FONT_SMALL, TINY_GREEN, ot);
	y += OPTIONS_MENU_ROW_HEIGHT;

	// Help
	DecalFont_DrawLineOT("L2/R2: Player   X: Select", OPTIONS_MENU_CENTER_X, y + 0x10, FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);

	// Border
	int listHeight = CONTROLS_ROW_COUNT * OPTIONS_MENU_ROW_HEIGHT;
	borders.x = OPTIONS_MENU_CENTER_X - OPTIONS_MENU_PANEL_W / 2 - 6;
	borders.y = OPTIONS_MENU_TITLE_Y - 8;
	borders.w = OPTIONS_MENU_PANEL_W + 12;
	borders.h = listHeight + (OPTIONS_MENU_LIST_TOP_Y - OPTIONS_MENU_TITLE_Y) + 0x30;
	RECTMENU_DrawInnerRect(&borders, 0, ot);
}

static void Options_HandleControlsInput(void)
{
	if (BTN_TAP_PLAYER0 & BTN_UP)
	{
		if (s_optionsSelectedIndex > 0)
			s_optionsSelectedIndex--;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & BTN_DOWN)
	{
		if (s_optionsSelectedIndex < CONTROLS_ROW_COUNT - 1)
			s_optionsSelectedIndex++;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & BTN_L2)
	{
		if (s_controlsPlayerIndex > 0)
			s_controlsPlayerIndex--;
		else
			s_controlsPlayerIndex = PLATFORM_INPUT_PLAYER_COUNT - 1;
		s_optionsSelectedIndex = 0;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & BTN_R2)
	{
		if (s_controlsPlayerIndex < PLATFORM_INPUT_PLAYER_COUNT - 1)
			s_controlsPlayerIndex++;
		else
			s_controlsPlayerIndex = 0;
		s_optionsSelectedIndex = 0;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}

	if (BTN_TAP_PLAYER0 & BTN_CROSS)
	{
		OtherFX_Play(1, 1);
		RECTMENU_ClearInput();

		if (s_optionsSelectedIndex == CONTROLS_ROW_DEVICE)
		{
			s_deviceListIndex = Options_GetDeviceForPlayer(s_controlsPlayerIndex);
			if (s_deviceListIndex < 0) s_deviceListIndex = 0;
			s_optionsState = OPTIONS_STATE_DEVICELIST;
		}
		else if (s_optionsSelectedIndex == CONTROLS_ROW_KEYBIND)
		{
			s_optionsBindSelectedIndex = 0;
			s_optionsBindScrollOffset = 0;
			s_optionsWaitingForKey = 0;
			s_optionsState = OPTIONS_STATE_KEYS;
		}
		else if (s_optionsSelectedIndex == CONTROLS_ROW_GPBIND)
		{
			s_optionsBindSelectedIndex = 0;
			s_optionsBindScrollOffset = 0;
			s_optionsWaitingForKey = 0;
			s_optionsState = OPTIONS_STATE_GAMEPAD;
		}
	}

	if (BTN_TAP_PLAYER0 & (BTN_TRIANGLE | BTN_SQUARE))
	{
		s_optionsState = OPTIONS_STATE_MAIN;
		s_optionsSelectedIndex = OPTION_ROW_CONTROLS;
		OtherFX_Play(2, 1);
		Options_ClearButtonTaps();
		RECTMENU_ClearInput();
	}
}

static void Options_DeviceList_ClampScroll(void)
{
	int hasNone = (s_controlsPlayerIndex != 0) ? 1 : 0;
	int totalRows = 1 + Platform_InputGetGamepadDeviceCount() + hasNone;
	int maxScroll;

	if (totalRows <= OPTIONS_MENU_VISIBLE_ROWS)
	{
		s_optionsBindScrollOffset = 0;
		return;
	}

	maxScroll = totalRows - OPTIONS_MENU_VISIBLE_ROWS;
	if (s_deviceListIndex < s_optionsBindScrollOffset)
		s_optionsBindScrollOffset = s_deviceListIndex;
	else if (s_deviceListIndex >= s_optionsBindScrollOffset + OPTIONS_MENU_VISIBLE_ROWS)
		s_optionsBindScrollOffset = s_deviceListIndex - OPTIONS_MENU_VISIBLE_ROWS + 1;
	if (s_optionsBindScrollOffset < 0)
		s_optionsBindScrollOffset = 0;
	if (s_optionsBindScrollOffset > maxScroll)
		s_optionsBindScrollOffset = maxScroll;
}

static int Options_DeviceList_GetCurrentIndex(void)
{
	int currentDevice = Options_GetDeviceForPlayer(s_controlsPlayerIndex);
	int hasNone = (s_controlsPlayerIndex != 0) ? 1 : 0;
	// List layout for P1:    Keyboard(0) Gamepad0(1) Gamepad1(2) ...
	// List layout for P2-P4: None(0) Keyboard(1) Gamepad0(2) Gamepad1(3) ...
	// currentDevice: 0=None 1=Keyboard 2+=Gamepad
	if (currentDevice <= 0)
		return hasNone ? 0 : -1;
	return currentDevice - 1 + hasNone;
}

static int Options_DeviceList_IndexToDevice(int listIndex)
{
	int hasNone = (s_controlsPlayerIndex != 0) ? 1 : 0;
	// P1:    list 0=Keyboard(1) list 1=Gamepad0(2) ...
	// P2-P4: list 0=None(0) list 1=Keyboard(1) list 2=Gamepad0(2) ...
	if (!hasNone)
		return listIndex + 1;
	return listIndex;
}

static void Options_DrawDeviceList(u_long *ot)
{
	int i;
	int y = OPTIONS_MENU_LIST_TOP_Y;
	int hasNone = (s_controlsPlayerIndex != 0) ? 1 : 0;
	int gamepadCount = Platform_InputGetGamepadDeviceCount();
	int totalRows = gamepadCount + 1 + hasNone;
	int visibleCount;
	RECT highlight;
	RECT borders;
	int currentListIndex = Options_DeviceList_GetCurrentIndex();
	char titleBuf[32];
	char rowLabels[OPTIONS_MENU_VISIBLE_ROWS + 1][32];

	Options_DeviceList_ClampScroll();

	snprintf(titleBuf, sizeof(titleBuf), "P%d DEVICE", s_controlsPlayerIndex + 1);
	DecalFont_DrawLineOT(titleBuf, OPTIONS_MENU_CENTER_X, OPTIONS_MENU_TITLE_Y + 0x10, FONT_SMALL, JUSTIFY_CENTER | TINY_GREEN, ot);

	// Highlight bar on cursor
	if (totalRows > 0)
	{
		int relIndex = s_deviceListIndex - s_optionsBindScrollOffset;
		highlight.x = OPTIONS_MENU_LEFT_X + 4;
		highlight.y = OPTIONS_MENU_LIST_TOP_Y + (relIndex * OPTIONS_MENU_ROW_HEIGHT) - 1;
		highlight.w = OPTIONS_MENU_HIGHLIGHT_W;
		highlight.h = OPTIONS_MENU_ROW_HEIGHT + 1;
		CTR_Box_DrawClearBox(&highlight, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
	}

	visibleCount = totalRows - s_optionsBindScrollOffset;
	if (visibleCount > OPTIONS_MENU_VISIBLE_ROWS)
		visibleCount = OPTIONS_MENU_VISIBLE_ROWS;

	for (i = 0; i < visibleCount; i++)
	{
		int absIndex = s_optionsBindScrollOffset + i;
		int device = Options_DeviceList_IndexToDevice(absIndex);
		const char *label;
		int color;

		if (device == 0)
			label = "None";
		else if (device == 1)
			label = "Keyboard";
		else
		{
			int gpIdx = device - 2;
			int joyId = Platform_InputGetGamepadDeviceId(gpIdx);
			if (joyId >= 0)
			{
				const char *name = Platform_InputGetGamepadDeviceName(joyId);
				if (name != NULL && name[0] != '\0')
					snprintf(rowLabels[i], sizeof(rowLabels[i]), "%.22s", name);
				else
					snprintf(rowLabels[i], sizeof(rowLabels[i]), "Gamepad %d", gpIdx + 1);
			}
			else
				snprintf(rowLabels[i], sizeof(rowLabels[i]), "Gamepad %d", gpIdx + 1);
			label = rowLabels[i];
		}

		color = (absIndex == currentListIndex) ? TINY_GREEN : WHITE;
		DecalFont_DrawLineOT((char *)label, OPTIONS_MENU_NAME_X + 0x10, y, FONT_SMALL, color, ot);
		if (absIndex == currentListIndex)
			DecalFont_DrawLineOT(">", OPTIONS_MENU_NAME_X, y, FONT_SMALL, TINY_GREEN, ot);

		y += OPTIONS_MENU_ROW_HEIGHT;
	}

	if (s_optionsBindScrollOffset > 0)
		DecalFont_DrawLineOT("^", OPTIONS_MENU_CENTER_X, OPTIONS_MENU_LIST_TOP_Y - 0x0C, FONT_SMALL, JUSTIFY_CENTER | WHITE, ot);
	if (totalRows > s_optionsBindScrollOffset + OPTIONS_MENU_VISIBLE_ROWS)
		DecalFont_DrawLineOT("v", OPTIONS_MENU_CENTER_X, y + 2, FONT_SMALL, JUSTIFY_CENTER | WHITE, ot);

	DecalFont_DrawLineOT("X: Select   TRI: Back", OPTIONS_MENU_CENTER_X, y + 0x1C, FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);

	int listHeight = (totalRows < OPTIONS_MENU_VISIBLE_ROWS ? totalRows : OPTIONS_MENU_VISIBLE_ROWS) * OPTIONS_MENU_ROW_HEIGHT;
	borders.x = OPTIONS_MENU_CENTER_X - OPTIONS_MENU_PANEL_W / 2 - 6;
	borders.y = OPTIONS_MENU_TITLE_Y - 8;
	borders.w = OPTIONS_MENU_PANEL_W + 12;
	borders.h = listHeight + (OPTIONS_MENU_LIST_TOP_Y - OPTIONS_MENU_TITLE_Y) + 0x30;
	RECTMENU_DrawInnerRect(&borders, 0, ot);
}

static void Options_HandleDeviceListInput(void)
{
	int hasNone = (s_controlsPlayerIndex != 0) ? 1 : 0;
	int gamepadCount = Platform_InputGetGamepadDeviceCount();
	int totalRows = gamepadCount + 1 + hasNone;
	int maxIndex = totalRows - 1;

	if (BTN_TAP_PLAYER0 & BTN_UP)
	{
		s_deviceListIndex--;
		if (s_deviceListIndex < 0)
			s_deviceListIndex = maxIndex;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & BTN_DOWN)
	{
		s_deviceListIndex++;
		if (s_deviceListIndex > maxIndex)
			s_deviceListIndex = 0;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & BTN_CROSS)
	{
		int device = Options_DeviceList_IndexToDevice(s_deviceListIndex);
		Options_SetDeviceForPlayer(s_controlsPlayerIndex, device);
		Options_SaveConfig();
		GAMEPAD_GetNumConnected(sdata->gGamepads);
		s_optionsState = OPTIONS_STATE_CONTROLS;
		OtherFX_Play(1, 1);
		RECTMENU_ClearInput();
	}
	else if (BTN_TAP_PLAYER0 & (BTN_TRIANGLE | BTN_SQUARE))
	{
		s_optionsState = OPTIONS_STATE_CONTROLS;
		OtherFX_Play(2, 1);
		Options_ClearButtonTaps();
		RECTMENU_ClearInput();
	}
}

void MM_Options_MenuProc(struct RectMenu *menu)
{
	struct GameTracker *gGT = sdata->gGT;
	u_long *ot = &gGT->backBuffer->otMem.startPlusFour[3];

	if (menu->rowSelected == -1)
	{
		if (menu->ptrPrevBox_InHierarchy != NULL)
			menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		s_optionsSelectedIndex = 0;
		s_optionsState = OPTIONS_STATE_MAIN;
		OtherFX_Play(2, 1);
		MM_JumpTo_Title_Returning();
		return;
	}

	DecalFont_DrawLineOT(
		sdata->lngStrings[LNG_OPTIONS],
		OPTIONS_MENU_CENTER_X, OPTIONS_MENU_TITLE_Y,
		FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);

	if (s_optionsState == OPTIONS_STATE_MAIN)
	{
		Options_DrawMain(gGT, ot);
		MM_Options_HandleMainInput();
	}
	else if (s_optionsState == OPTIONS_STATE_CONTROLS)
	{
		Options_DrawControls(ot);
		Options_HandleControlsInput();
	}
	else if (s_optionsState == OPTIONS_STATE_DEVICELIST)
	{
		Options_DrawDeviceList(ot);
		Options_HandleDeviceListInput();
	}
	else if (s_optionsState == OPTIONS_STATE_KEYS || s_optionsState == OPTIONS_STATE_WAITING)
	{
		Options_DrawKeys(ot);
		Options_HandleKeysInput();
	}
	else if (s_optionsState == OPTIONS_STATE_GAMEPAD || s_optionsState == OPTIONS_STATE_WAITING_GAMEPAD)
	{
		Options_DrawGamepad(ot);
		Options_HandleGamepadInput();
	}
}
