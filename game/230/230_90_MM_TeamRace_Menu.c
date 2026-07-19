#include <common.h>

static struct MenuRow s_teamRaceMenuRows[] = { {-1} };

static struct RectMenu s_teamRaceMenu =
{
	.stringIndexTitle = 0x15D,
	.state            = 0x28,
	.funcPtr          = MM_TeamRace_MenuProc,
	.rows             = s_teamRaceMenuRows,
};

static int s_trSelectedIndex = 0;

struct RectMenu *MM_TeamRace_GetMenuPtr(void)
{
	return &s_teamRaceMenu;
}

void MM_TeamRace_MenuProc(struct RectMenu *menu)
{
	struct GameTracker *gGT = sdata->gGT;
	u_long *ot = &gGT->backBuffer->otMem.startPlusFour[3];
	int y;
	int i;

	if (menu->rowSelected == -1)
	{
		if (menu->ptrPrevBox_InHierarchy != NULL)
			menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		s_trSelectedIndex = 0;
		OtherFX_Play(2, 1);
		return;
	}

	DecalFont_DrawLineOT(
		sdata->lngStrings[0x15D],
		0x100, 0x1C,
		FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);

	{
		RECT hl = { 0x84, (short)(0x3C + s_trSelectedIndex * 0x10 - 1), 0x198, 0x11 };
		CTR_Box_DrawClearBox(&hl, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
	}

	static const char *s_trNames[3] = { "SINGLE", "CUP", "TEAM RACE" };
	y = 0x3C;
	for (i = 0; i < 3; i++)
	{
		DecalFont_DrawLineOT(
			(char *)s_trNames[i],
			0x100, y,
			FONT_SMALL, JUSTIFY_CENTER | (i == s_trSelectedIndex ? WHITE : ORANGE), ot);
		y += 0x10;
	}

	if (sdata->buttonTapPerPlayer[0] & BTN_UP)
	{
		s_trSelectedIndex = (s_trSelectedIndex - 1 + 3) % 3;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (sdata->buttonTapPerPlayer[0] & BTN_DOWN)
	{
		s_trSelectedIndex = (s_trSelectedIndex + 1) % 3;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
	}
	else if (sdata->buttonTapPerPlayer[0] & BTN_CROSS)
	{
		RECTMENU_ClearInput();
		OtherFX_Play(1, 1);

		gGT->gameMode2 &= ~(CUP_ANY_KIND | TEAM_RACE_MODE);

		if (s_trSelectedIndex <= 1)
		{
			if (s_trSelectedIndex == 1)
				gGT->gameMode2 |= CUP_ANY_KIND;

			menu->state |= 0x14;
			menu->ptrNextBox_InHierarchy = &D230.menuPlayers1P2P;
			D230.characterSelect_transitionState = 1;
			return;
		}
		else
		{
			gGT->gameMode2 |= TEAM_RACE_MODE;
			gGT->numPlyrNextGame = 1;
			D230.characterSelect_transitionState = 1;
			D230.MM_State = 2;
			D230.desiredMenuIndex = 2;
			return;
		}
	}
	else if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE | BTN_CIRCLE))
	{
		OtherFX_Play(2, 1);
		RECTMENU_ClearInput();
		if (menu->ptrPrevBox_InHierarchy != NULL)
			menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		sdata->ptrDesiredMenu = &D230.menuMainMenu;
	}
}
