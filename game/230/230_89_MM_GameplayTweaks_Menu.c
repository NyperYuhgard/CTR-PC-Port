#include <common.h>
#include <platform/native_config.h>

// ============================================================================
// Gameplay Tweaks Submenu – Options > Gameplay Tweaks
//
// Visual style mirrors the MODS menu (full-screen CTR-native panel).
// Reachable from the Options menu; returns to Options on back.
//
// NOTE on PS1 OT draw order: draw content (text/highlight) FIRST, then the
// panel border LAST so the semi-transparent fill appears behind the text.
// ============================================================================

#define GT_MENU_VISIBLE_ROWS 8

#define GT_MENU_CENTER_X     0x100
#define GT_MENU_PANEL_W      0x1C0

#define GT_MENU_TITLE_Y      0x1C
#define GT_MENU_LIST_TOP_Y   0x3C
#define GT_MENU_ROW_HEIGHT   0x10
#define GT_MENU_HELP_Y       0xB4

#define GT_MENU_LEFT_X      (GT_MENU_CENTER_X - GT_MENU_PANEL_W / 2)
#define GT_MENU_RIGHT_X     (GT_MENU_CENTER_X + GT_MENU_PANEL_W / 2)
#define GT_MENU_NAME_X      (GT_MENU_LEFT_X + 0x10)
#define GT_MENU_STATUS_X    (GT_MENU_RIGHT_X - 0x10)
#define GT_MENU_HIGHLIGHT_W (GT_MENU_PANEL_W - 0x20)

#define GT_COUNT 5

static int s_gtSelectedIndex = 0;
static int s_gtScrollOffset  = 0;

static const char *s_gtNames[GT_COUNT] =
{
	"Special Items",
	"CPU All Items",
	"Item Chaos",
	"CPU Item Chaos",
	"Chaos RNG",
};

static int *s_gtValues[GT_COUNT] =
{
	&g_cfg_specialItems,
	&g_cfg_cpuAllItems,
	&g_cfg_itemChaos,
	&g_cfg_cpuItemChaos,
	&g_cfg_chaosRng,
};

// Self-contained menu (no OverlayDATA_230 field needed).
static struct RectMenu s_gameplayTweaksMenu =
{
	.stringIndexTitle = 0,
	.state            = 0x28,
	.funcPtr          = MM_GameplayTweaks_MenuProc,
};

void MM_GameplayTweaks_Init(void)
{
	s_gtSelectedIndex = 0;
	s_gtScrollOffset  = 0;
	s_gameplayTweaksMenu.state &= ~NEEDS_TO_CLOSE;
}

struct RectMenu *MM_GameplayTweaks_GetMenuPtr(void)
{
	return &s_gameplayTweaksMenu;
}

static void GameplayTweaks_ClampScroll(void)
{
	int maxScroll;

	if (GT_COUNT <= GT_MENU_VISIBLE_ROWS)
	{
		s_gtScrollOffset = 0;
		return;
	}

	maxScroll = GT_COUNT - GT_MENU_VISIBLE_ROWS;

	if (s_gtSelectedIndex < s_gtScrollOffset)
		s_gtScrollOffset = s_gtSelectedIndex;
	else if (s_gtSelectedIndex >= s_gtScrollOffset + GT_MENU_VISIBLE_ROWS)
		s_gtScrollOffset = s_gtSelectedIndex - GT_MENU_VISIBLE_ROWS + 1;

	if (s_gtScrollOffset < 0)
		s_gtScrollOffset = 0;
	if (s_gtScrollOffset > maxScroll)
		s_gtScrollOffset = maxScroll;
}

void MM_GameplayTweaks_MenuProc(struct RectMenu *menu)
{
	struct GameTracker *gGT = sdata->gGT;
	u_long *ot = &gGT->backBuffer->otMem.startPlusFour[3];
	int i;
	int y;
	int visibleCount;
	int listHeight;
	RECT highlight;
	RECT borders;

	// ---- Back to Options ----
	if (menu->rowSelected == -1)
	{
		if (menu->ptrPrevBox_InHierarchy != NULL)
			menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		s_gtSelectedIndex = 0;
		s_gtScrollOffset  = 0;
		OtherFX_Play(2, 1);
		sdata->ptrDesiredMenu = &D230.menuOptions;
		return;
	}

	GameplayTweaks_ClampScroll();

	// ---- Title ----
	DecalFont_DrawLineOT(
		"GAMEPLAY TWEAKS",
		GT_MENU_CENTER_X, GT_MENU_TITLE_Y,
		FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);

	// ---- Selection highlight bar ----
	if (GT_COUNT > 0)
	{
		int relIndex = s_gtSelectedIndex - s_gtScrollOffset;

		highlight.x = GT_MENU_LEFT_X + 4;
		highlight.y = GT_MENU_LIST_TOP_Y + (relIndex * GT_MENU_ROW_HEIGHT) - 1;
		highlight.w = GT_MENU_HIGHLIGHT_W;
		highlight.h = GT_MENU_ROW_HEIGHT + 1;

		CTR_Box_DrawClearBox(
			&highlight,
			&sdata->menuRowHighlight_Normal,
			TRANS_50_DECAL, ot);
	}

	// ---- Rows ----
	y = GT_MENU_LIST_TOP_Y;
	visibleCount = GT_COUNT - s_gtScrollOffset;
	if (visibleCount > GT_MENU_VISIBLE_ROWS)
		visibleCount = GT_MENU_VISIBLE_ROWS;

	for (i = 0; i < visibleCount; i++)
	{
		int absIndex   = s_gtScrollOffset + i;
		int isSelected = (absIndex == s_gtSelectedIndex);
		int on         = *s_gtValues[absIndex];
		int nameColor  = isSelected ? WHITE : ORANGE;
		int statusColor = on ? TINY_GREEN : RED;

		DecalFont_DrawLineOT(
			(char *)s_gtNames[absIndex],
			GT_MENU_NAME_X, y,
			FONT_SMALL, nameColor, ot);

		DecalFont_DrawLineOT(
			on ? "ON" : "OFF",
			GT_MENU_STATUS_X, y,
			FONT_SMALL, statusColor, ot);

		y += GT_MENU_ROW_HEIGHT;
	}

	// ---- Scroll indicators ----
	if (s_gtScrollOffset > 0)
	{
		DecalFont_DrawLineOT(
			"^",
			GT_MENU_CENTER_X, GT_MENU_LIST_TOP_Y - 0x0C,
			FONT_SMALL, JUSTIFY_CENTER | WHITE, ot);
	}
	if (GT_COUNT > s_gtScrollOffset + GT_MENU_VISIBLE_ROWS)
	{
		DecalFont_DrawLineOT(
			"v",
			GT_MENU_CENTER_X, y + 2,
			FONT_SMALL, JUSTIFY_CENTER | WHITE, ot);
	}

	// ---- Help bar ----
	DecalFont_DrawLineOT(
		"UP/DN: Select   X: Toggle   TRI: Back",
		GT_MENU_CENTER_X, GT_MENU_HELP_Y,
		FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);

	// ---- Panel border LAST ----
	listHeight = GT_MENU_VISIBLE_ROWS * GT_MENU_ROW_HEIGHT;
	borders.x = GT_MENU_CENTER_X - GT_MENU_PANEL_W / 2 - 6;
	borders.y = GT_MENU_TITLE_Y - 8;
	borders.w = GT_MENU_PANEL_W + 12;
	borders.h = listHeight + (GT_MENU_LIST_TOP_Y - GT_MENU_TITLE_Y) + 0x30;
	RECTMENU_DrawInnerRect(&borders, 0, ot);

	// ---- Input handling ----
	if (sdata->buttonTapPerPlayer[0] & BTN_UP)
	{
		s_gtSelectedIndex = (s_gtSelectedIndex - 1 + GT_COUNT) % GT_COUNT;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (sdata->buttonTapPerPlayer[0] & BTN_DOWN)
	{
		s_gtSelectedIndex = (s_gtSelectedIndex + 1) % GT_COUNT;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (sdata->buttonTapPerPlayer[0] & BTN_CROSS)
	{
		if (s_gtSelectedIndex < GT_COUNT)
		{
			*s_gtValues[s_gtSelectedIndex] = *s_gtValues[s_gtSelectedIndex] ? 0 : 1;
			NativeConfig_Save();
			OtherFX_Play(1, 1);
		}
		RECTMENU_ClearInput();
	}
	else if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
	{
		OtherFX_Play(2, 1);
		RECTMENU_ClearInput();
		sdata->ptrDesiredMenu = &D230.menuOptions;
	}
}
