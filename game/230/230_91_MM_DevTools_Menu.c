#ifdef CTR_NATIVE_DEV_HUD_EDITOR

#include <common.h>
#include <platform/native_hud_editor.h>

#define DT_MENU_CENTER_X     0x100
#define DT_MENU_PANEL_W      0x1C0
#define DT_MENU_TITLE_Y      0x1C
#define DT_MENU_LIST_TOP_Y   0x3C
#define DT_MENU_ROW_HEIGHT   0x10
#define DT_MENU_LEFT_X       (DT_MENU_CENTER_X - DT_MENU_PANEL_W / 2)
#define DT_MENU_RIGHT_X      (DT_MENU_CENTER_X + DT_MENU_PANEL_W / 2)
#define DT_MENU_NAME_X       (DT_MENU_LEFT_X + 0x10)
#define DT_MENU_STATUS_X     (DT_MENU_RIGHT_X - 0x10)
#define DT_MENU_HIGHLIGHT_W  (DT_MENU_PANEL_W - 0x20)
#define DT_MENU_HELP_Y       0xB4

#define DT_COUNT 2

static int s_dtSelectedIndex = 0;

static const char *s_dtNames[DT_COUNT] =
{
	"HUD Layout Editor",
	"Quick Race (Crash Cove)",
};

static struct RectMenu s_devToolsMenu =
{
	.stringIndexTitle = 0,
	.state            = 0x28,
	.funcPtr          = MM_DevTools_MenuProc,
};

void MM_DevTools_Init(void)
{
	s_dtSelectedIndex = 0;
	s_devToolsMenu.state &= ~NEEDS_TO_CLOSE;
}

struct RectMenu *MM_DevTools_GetMenuPtr(void)
{
	return &s_devToolsMenu;
}

void MM_DevTools_MenuProc(struct RectMenu *menu)
{
	struct GameTracker *gGT = sdata->gGT;
	u_long *ot = &gGT->backBuffer->otMem.startPlusFour[3];
	int y;
	RECT highlight;
	RECT borders;

	if (menu->rowSelected == -1)
	{
		if (menu->ptrPrevBox_InHierarchy != NULL)
			menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		s_dtSelectedIndex = 0;
		OtherFX_Play(2, 1);
		sdata->ptrDesiredMenu = &D230.menuOptions;
		return;
	}

	if (HudEditor_IsActive())
	{
		HudEditor_HandleInput();
		HudEditor_Render();
		return;
	}

	DecalFont_DrawLineOT(
		"DEV TOOLS",
		DT_MENU_CENTER_X, DT_MENU_TITLE_Y,
		FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);

	if (DT_COUNT > 0)
	{
		int relIndex = s_dtSelectedIndex;
		highlight.x = DT_MENU_LEFT_X + 4;
		highlight.y = DT_MENU_LIST_TOP_Y + (relIndex * DT_MENU_ROW_HEIGHT) - 1;
		highlight.w = DT_MENU_HIGHLIGHT_W;
		highlight.h = DT_MENU_ROW_HEIGHT + 1;
		CTR_Box_DrawClearBox(&highlight, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
	}

	y = DT_MENU_LIST_TOP_Y;
	for (int i = 0; i < DT_COUNT; i++)
	{
		int isSel = (i == s_dtSelectedIndex);
		DecalFont_DrawLineOT(
			(char *)s_dtNames[i],
			DT_MENU_NAME_X, y,
			FONT_SMALL, isSel ? WHITE : ORANGE, ot);

		if (i == 0)
		{
			DecalFont_DrawLineOT(
				"OPEN",
				DT_MENU_STATUS_X, y,
				FONT_SMALL, TINY_GREEN, ot);
		}
		else if (i == 1)
		{
			DecalFont_DrawLineOT(
				"START",
				DT_MENU_STATUS_X, y,
				FONT_SMALL, TINY_GREEN, ot);
		}

		y += DT_MENU_ROW_HEIGHT;
	}

	DecalFont_DrawLineOT(
		"UP/DN: Select   X: Action   TRI: Back",
		DT_MENU_CENTER_X, DT_MENU_HELP_Y,
		FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);

	int listHeight = DT_COUNT * DT_MENU_ROW_HEIGHT;
	borders.x = DT_MENU_CENTER_X - DT_MENU_PANEL_W / 2 - 6;
	borders.y = DT_MENU_TITLE_Y - 8;
	borders.w = DT_MENU_PANEL_W + 12;
	borders.h = listHeight + (DT_MENU_LIST_TOP_Y - DT_MENU_TITLE_Y) + 0x30;
	RECTMENU_DrawInnerRect(&borders, 0, ot);

	if (sdata->buttonTapPerPlayer[0] & BTN_UP)
	{
		s_dtSelectedIndex = (s_dtSelectedIndex - 1 + DT_COUNT) % DT_COUNT;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (sdata->buttonTapPerPlayer[0] & BTN_DOWN)
	{
		s_dtSelectedIndex = (s_dtSelectedIndex + 1) % DT_COUNT;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (sdata->buttonTapPerPlayer[0] & BTN_CROSS)
	{
		if (s_dtSelectedIndex == 0)
		{
			gGT->gameMode2 |= NO_AI_RACE;
			HudEditor_Init();
			HudEditor_Activate();
			OtherFX_Play(1, 1);
		}
		else if (s_dtSelectedIndex == 1)
		{
			gGT->gameMode1 &= ~(BATTLE_MODE | ADVENTURE_MODE | TIME_TRIAL | ADVENTURE_ARENA | ADVENTURE_CUP);
			gGT->gameMode2 &= ~(CUP_ANY_KIND | COOPERATIVE_ADVENTURE);
			gGT->gameMode2 |= NO_AI_RACE;
			gGT->gameMode1 |= ARCADE_MODE;
			gGT->numPlyrCurrGame = 1;
			gGT->numLaps = 3;
			data.characterIDs[0] = CRASH_BANDICOOT;
			gGT->currLEV = CRASH_COVE;

			HudEditor_Init();
			HudEditor_Activate();

			MainRaceTrack_RequestLoad(CRASH_COVE);
			RECTMENU_Hide(menu);

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

#endif
