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
#define OPTION_ROW_COUNT     11

#define ASPECT_4_3     0
#define ASPECT_16_9    1
#define ASPECT_STRETCH 2
#define ASPECT_16_9_WS 3

#define OPTIONS_STATE_MAIN    0
#define OPTIONS_STATE_KEYS    1
#define OPTIONS_STATE_WAITING 2

static int s_optionsSelectedIndex = 0;
static int s_optionsState = OPTIONS_STATE_MAIN;
static int s_optionsScrollOffset = 0;
static int s_optionsBindScrollOffset = 0;
static int s_optionsBindSelectedIndex = 0;
static int s_optionsWaitingForKey = 0;
static int s_optionsPrevKeyboardState[576]; // SDL_SCANCODE_COUNT max
static int s_optionsPrevKeyboardStateCount;

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
			s_optionsBindSelectedIndex = 0;
			s_optionsBindScrollOffset = 0;
			s_optionsWaitingForKey = 0;
			s_optionsState = OPTIONS_STATE_KEYS;
		}
		else if (s_optionsSelectedIndex == OPTION_ROW_GAMETWEAKS)
		{
			MM_GameplayTweaks_Init();
			RECTMENU_ClearInput();
			sdata->ptrDesiredMenu = MM_GameplayTweaks_GetMenuPtr();
		}
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

		if (!Platform_InputGetKeyBinding(absIndex, &scancode))
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
		Platform_InputGetKeyBinding(s_optionsBindSelectedIndex, &actionScancode);
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
				Platform_InputSetKeyBinding(s_optionsBindSelectedIndex, i);
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
		s_optionsState = OPTIONS_STATE_MAIN;
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
	else
	{
		Options_DrawKeys(ot);
		Options_HandleKeysInput();
	}
}
