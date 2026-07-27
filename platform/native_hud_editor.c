#include <common.h>

#ifdef CTR_NATIVE_DEV_HUD_EDITOR

#include <platform/native_hud_editor.h>
#include <stdio.h>
#include <string.h>

static struct HudEditorState s_hudEditor;
static Uint8 s_prevKeyState[SDL_SCANCODE_COUNT];

static const char *s_elementNames[HUD_EDITOR_TOTAL_ELEMENTS] =
{
	"Weapon Icon",
	"Lap Counter",
	"Rank Number",
	"Wumpa 3D Pos",
	"Wumpa Count",
	"Pos Suffix",
	"Jump Meter",
	"Speed BG",
	"Slide Meter",
	"Speedometer",
	"Unused 0A",
	"Weapon Shine",
	"Wumpa Shine",
	"Battle Lives",
	"Relic Count",
	"Key Count",
	"Trophy Count",
	"Crystal Count",
	"CTR Letters",
	"Time Crate",
	"Team Icon",
	"Team Text",
	"Team Bar",
};

static const char *s_gamemodeNames[] =
{
	"Arcade",
	"Battle",
	"Time Trial",
	"Relic Race",
	"Team Race",
};
#define GAMEMODE_COUNT 5

static const char *s_playerCountNames[] = { "", "1P", "2P", "3P", "4P" };

static int HandleKeyDown(int scancode)
{
	return Platform_InputIsKeyDown(scancode);
}

static int HandleKeyTap(int scancode)
{
	const Uint8 *curr = SDL_GetKeyboardState(NULL);
	int tapped = curr[scancode] && !s_prevKeyState[scancode];
	return tapped;
}

static void HandleKeyTapUpdate(void)
{
	const Uint8 *curr = SDL_GetKeyboardState(NULL);
	memcpy(s_prevKeyState, curr, sizeof(s_prevKeyState));
}

static void HudEditor_ReloadBaseOffsets(void)
{
	int plyr = s_hudEditor.playerCount;
	if (plyr < 1) plyr = 1;
	if (plyr > 4) plyr = 4;

	for (int p = 0; p < plyr; p++)
	{
		struct UiElement2D *base = (struct UiElement2D *)data.hudStructPtr[plyr - 1];
		struct UiElement2D *src = &base[p * HUD_EDITOR_MAX_ELEMENTS];
		memcpy(s_hudEditor.offsets[p], src, HUD_EDITOR_MAX_ELEMENTS * sizeof(struct UiElement2D));
	}
}

static void HudEditor_RestartRace(void)
{
	struct GameTracker *gGT = sdata->gGT;
	MainRaceTrack_RequestLoad(gGT->currLEV);
}

static void HudEditor_ApplyGamemode(void)
{
	struct GameTracker *gGT = sdata->gGT;

	gGT->gameMode1 &= ~(BATTLE_MODE | TIME_TRIAL | RELIC_RACE | ARCADE_MODE | ADVENTURE_MODE | TIME_LIMIT | POINT_LIMIT | LIFE_LIMIT);
	gGT->gameMode2 &= ~TEAM_RACE_MODE;

	switch (s_hudEditor.gamemodeIndex)
	{
		case 0: // Arcade
			gGT->gameMode1 |= ARCADE_MODE;
			break;
		case 1: // Battle
			gGT->gameMode1 |= BATTLE_MODE | TIME_LIMIT;
			break;
		case 2: // Time Trial
			gGT->gameMode1 |= TIME_TRIAL;
			break;
		case 3: // Relic Race
			gGT->gameMode1 |= RELIC_RACE;
			break;
		case 4: // Team Race
			gGT->gameMode1 |= ARCADE_MODE;
			gGT->gameMode2 |= TEAM_RACE_MODE;
			s_hudEditor.teamRaceMode = 1;
			break;
	}

	if (s_hudEditor.gamemodeIndex != 4)
		s_hudEditor.teamRaceMode = (gGT->gameMode2 & TEAM_RACE_MODE) != 0;
}

void HudEditor_Init(void)
{
	struct GameTracker *gGT = sdata->gGT;
	int plyr = gGT->numPlyrCurrGame;
	if (plyr < 1) plyr = 1;
	if (plyr > 4) plyr = 4;

	memset(&s_hudEditor, 0, sizeof(s_hudEditor));
	s_hudEditor.playerCount = plyr;
	s_hudEditor.editPlayer = 0;
	s_hudEditor.teamRaceMode = 0;
	s_hudEditor.teamIconOffset[0] = -48;
	s_hudEditor.teamIconOffset[1] = -40;
	s_hudEditor.teamTextOffset[0] = -29;
	s_hudEditor.teamTextOffset[1] = -47;
	s_hudEditor.teamBarOffset[0] = -94;
	s_hudEditor.teamBarOffset[1] = -37;
	s_hudEditor.teamBarSize[0] = 12;
	s_hudEditor.teamBarSize[1] = 60;

	for (int p = 0; p < plyr; p++)
	{
		struct UiElement2D *base = (struct UiElement2D *)data.hudStructPtr[plyr - 1];
		struct UiElement2D *src = &base[p * HUD_EDITOR_MAX_ELEMENTS];
		memcpy(s_hudEditor.offsets[p], src, HUD_EDITOR_MAX_ELEMENTS * sizeof(struct UiElement2D));
	}

	memcpy(s_hudEditor.baseOffsets, s_hudEditor.offsets, sizeof(s_hudEditor.offsets));

	HudEditor_Load();
}

void HudEditor_Activate(void)
{
	struct GameTracker *gGT = sdata->gGT;
	int plyr = gGT->numPlyrCurrGame;
	if (plyr < 1) plyr = 1;
	if (plyr > 4) plyr = 4;
	s_hudEditor.playerCount = plyr;
	s_hudEditor.active = 1;
	s_hudEditor.visible = 1;
	s_hudEditor.editMode = 0;
	s_hudEditor.selectedElement = 0;
	s_hudEditor.editPlayer = 0;
	s_hudEditor.helpPage = 0;
}

void HudEditor_Deactivate(void)
{
	s_hudEditor.active = 0;
	s_hudEditor.visible = 0;
}

int HudEditor_IsActive(void)
{
	return s_hudEditor.active && (sdata->gGT->gameMode2 & NO_AI_RACE) != 0;
}

struct HudEditorState *HudEditor_GetState(void)
{
	return &s_hudEditor;
}

struct UiElement2D *HudEditor_GetOffsets(void)
{
	return (struct UiElement2D *)s_hudEditor.offsets;
}

void HudEditor_GetTeamOffsets(s16 *iconOff, s16 *textOff, s16 *barOff, s16 *barSize)
{
	if (iconOff) { iconOff[0] = s_hudEditor.teamIconOffset[0]; iconOff[1] = s_hudEditor.teamIconOffset[1]; }
	if (textOff) { textOff[0] = s_hudEditor.teamTextOffset[0]; textOff[1] = s_hudEditor.teamTextOffset[1]; }
	if (barOff)  { barOff[0] = s_hudEditor.teamBarOffset[0];   barOff[1] = s_hudEditor.teamBarOffset[1]; }
	if (barSize) { barSize[0] = s_hudEditor.teamBarSize[0];    barSize[1] = s_hudEditor.teamBarSize[1]; }
}

void HudEditor_HandleInput(void)
{
	struct GameTracker *gGT = sdata->gGT;
	struct HudEditorState *st = &s_hudEditor;
	int shift = HandleKeyDown(SDL_SCANCODE_LSHIFT) || HandleKeyDown(SDL_SCANCODE_RSHIFT);
	int ctrl = HandleKeyDown(SDL_SCANCODE_LCTRL) || HandleKeyDown(SDL_SCANCODE_RCTRL);
	int step = ctrl ? 16 : (shift ? 4 : 1);
	int ep = st->editPlayer;

	if (!st->active)
	{
		HandleKeyTapUpdate();
		return;
	}

	if (HandleKeyTap(SDL_SCANCODE_TAB))
	{
		st->editMode = !st->editMode;
		OtherFX_Play(st->editMode ? 1 : 2, 1);
		RECTMENU_ClearInput();
		sdata->buttonTapPerPlayer[0] = 0;
		sdata->gGamepads->gamepad[0].buttonsTapped = 0;
		HandleKeyTapUpdate();
		return;
	}

	if (HandleKeyTap(SDL_SCANCODE_ESCAPE))
	{
		HudEditor_Deactivate();
		OtherFX_Play(2, 1);
		RECTMENU_ClearInput();
		HandleKeyTapUpdate();
		return;
	}

	if (!st->editMode)
	{
		HandleKeyTapUpdate();
		return;
	}

	// --- Element selection ---
	if (HandleKeyTap(SDL_SCANCODE_R))
	{
		st->selectedElement = (st->selectedElement + 1) % HUD_EDITOR_TOTAL_ELEMENTS;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
		HandleKeyTapUpdate();
		return;
	}
	if (HandleKeyTap(SDL_SCANCODE_Q))
	{
		st->selectedElement = (st->selectedElement - 1 + HUD_EDITOR_TOTAL_ELEMENTS) % HUD_EDITOR_TOTAL_ELEMENTS;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
		HandleKeyTapUpdate();
		return;
	}

	// --- Player selection ---
	if (HandleKeyTap(SDL_SCANCODE_P))
	{
		st->editPlayer = (st->editPlayer + 1) % st->playerCount;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
		HandleKeyTapUpdate();
		return;
	}

	// --- Toggle element visibility ---
	if (HandleKeyTap(SDL_SCANCODE_N))
	{
		st->hidden[ep][st->selectedElement] = !st->hidden[ep][st->selectedElement];
		st->modified = 1;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
		HandleKeyTapUpdate();
		return;
	}

	// --- Reset selected element to base ---
	if (HandleKeyTap(SDL_SCANCODE_Z))
	{
		if (st->selectedElement < HUD_EDITOR_MAX_ELEMENTS)
		{
			memcpy(&st->offsets[ep][st->selectedElement],
				   &st->baseOffsets[ep][st->selectedElement],
				   sizeof(struct UiElement2D));
			st->hidden[ep][st->selectedElement] = 0;
		}
		st->modified = 1;
		OtherFX_Play(1, 1);
		RECTMENU_ClearInput();
		HandleKeyTapUpdate();
		return;
	}

	// --- Reset ALL elements for current player ---
	if (HandleKeyTap(SDL_SCANCODE_F))
	{
		memcpy(st->offsets[ep], st->baseOffsets[ep],
			   HUD_EDITOR_MAX_ELEMENTS * sizeof(struct UiElement2D));
		memset(st->hidden[ep], 0, sizeof(st->hidden[ep]));
		st->modified = 1;
		OtherFX_Play(1, 1);
		RECTMENU_ClearInput();
		HandleKeyTapUpdate();
		return;
	}

	// --- Copy current element to ALL players ---
	if (shift && HandleKeyTap(SDL_SCANCODE_C))
	{
		if (st->selectedElement < HUD_EDITOR_MAX_ELEMENTS)
		{
			for (int p = 0; p < st->playerCount; p++)
			{
				memcpy(&st->offsets[p][st->selectedElement],
					   &st->offsets[ep][st->selectedElement],
					   sizeof(struct UiElement2D));
			}
		}
		st->modified = 1;
		OtherFX_Play(1, 1);
		RECTMENU_ClearInput();
		HandleKeyTapUpdate();
		return;
	}

	// --- Save (C without shift) ---
	if (!shift && HandleKeyTap(SDL_SCANCODE_C))
	{
		HudEditor_Save();
		OtherFX_Play(1, 1);
		RECTMENU_ClearInput();
		HandleKeyTapUpdate();
		return;
	}

	// --- Load ---
	if (HandleKeyTap(SDL_SCANCODE_V))
	{
		HudEditor_Load();
		OtherFX_Play(1, 1);
		RECTMENU_ClearInput();
		HandleKeyTapUpdate();
		return;
	}

	// --- Team race toggle ---
	if (HandleKeyTap(SDL_SCANCODE_X))
	{
		st->teamRaceMode = !st->teamRaceMode;
		if (st->teamRaceMode)
			gGT->gameMode2 |= TEAM_RACE_MODE;
		else
			gGT->gameMode2 &= ~TEAM_RACE_MODE;
		OtherFX_Play(1, 1);
		RECTMENU_ClearInput();
		HandleKeyTapUpdate();
		return;
	}

	// --- Gamemode ---
	if (HandleKeyTap(SDL_SCANCODE_G))
	{
		st->gamemodeIndex = (st->gamemodeIndex + 1) % GAMEMODE_COUNT;
		HudEditor_ApplyGamemode();
		st->modified = 1;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
		HandleKeyTapUpdate();
		return;
	}

	// --- Help ---
	if (HandleKeyTap(SDL_SCANCODE_H))
	{
		st->helpPage = !st->helpPage;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
		HandleKeyTapUpdate();
		return;
	}

	// --- Player count + restart ---
	if (HandleKeyTap(SDL_SCANCODE_1)) { st->playerCount = 1; gGT->numPlyrNextGame = 1; HudEditor_ReloadBaseOffsets(); memcpy(st->baseOffsets, st->offsets, sizeof(st->baseOffsets)); HudEditor_RestartRace(); st->modified = 1; OtherFX_Play(0, 1); RECTMENU_ClearInput(); HandleKeyTapUpdate(); return; }
	if (HandleKeyTap(SDL_SCANCODE_2)) { st->playerCount = 2; gGT->numPlyrNextGame = 2; HudEditor_ReloadBaseOffsets(); memcpy(st->baseOffsets, st->offsets, sizeof(st->baseOffsets)); HudEditor_RestartRace(); st->modified = 1; OtherFX_Play(0, 1); RECTMENU_ClearInput(); HandleKeyTapUpdate(); return; }
	if (HandleKeyTap(SDL_SCANCODE_3)) { st->playerCount = 3; gGT->numPlyrNextGame = 3; HudEditor_ReloadBaseOffsets(); memcpy(st->baseOffsets, st->offsets, sizeof(st->baseOffsets)); HudEditor_RestartRace(); st->modified = 1; OtherFX_Play(0, 1); RECTMENU_ClearInput(); HandleKeyTapUpdate(); return; }
	if (HandleKeyTap(SDL_SCANCODE_4)) { st->playerCount = 4; gGT->numPlyrNextGame = 4; HudEditor_ReloadBaseOffsets(); memcpy(st->baseOffsets, st->offsets, sizeof(st->baseOffsets)); HudEditor_RestartRace(); st->modified = 1; OtherFX_Play(0, 1); RECTMENU_ClearInput(); HandleKeyTapUpdate(); return; }

	// --- Movement (continuous) ---
	if (HandleKeyDown(SDL_SCANCODE_UP))
	{
		if (st->selectedElement >= HUD_EDITOR_TEAM_BASE)
		{
			int t = st->selectedElement - HUD_EDITOR_TEAM_BASE;
			if (t == 0) st->teamIconOffset[1] -= step;
			else if (t == 1) st->teamTextOffset[1] -= step;
			else if (t == 2) st->teamBarOffset[1] -= step;
		}
		else
			st->offsets[ep][st->selectedElement][1] -= step;
		st->modified = 1;
	}
	if (HandleKeyDown(SDL_SCANCODE_DOWN))
	{
		if (st->selectedElement >= HUD_EDITOR_TEAM_BASE)
		{
			int t = st->selectedElement - HUD_EDITOR_TEAM_BASE;
			if (t == 0) st->teamIconOffset[1] += step;
			else if (t == 1) st->teamTextOffset[1] += step;
			else if (t == 2) st->teamBarOffset[1] += step;
		}
		else
			st->offsets[ep][st->selectedElement][1] += step;
		st->modified = 1;
	}
	if (HandleKeyDown(SDL_SCANCODE_LEFT))
	{
		if (st->selectedElement >= HUD_EDITOR_TEAM_BASE)
		{
			int t = st->selectedElement - HUD_EDITOR_TEAM_BASE;
			if (t == 0) st->teamIconOffset[0] -= step;
			else if (t == 1) st->teamTextOffset[0] -= step;
			else if (t == 2) st->teamBarOffset[0] -= step;
		}
		else
			st->offsets[ep][st->selectedElement][0] -= step;
		st->modified = 1;
	}
	if (HandleKeyDown(SDL_SCANCODE_RIGHT))
	{
		if (st->selectedElement >= HUD_EDITOR_TEAM_BASE)
		{
			int t = st->selectedElement - HUD_EDITOR_TEAM_BASE;
			if (t == 0) st->teamIconOffset[0] += step;
			else if (t == 1) st->teamTextOffset[0] += step;
			else if (t == 2) st->teamBarOffset[0] += step;
		}
		else
			st->offsets[ep][st->selectedElement][0] += step;
		st->modified = 1;
	}

	// --- Scale (continuous) ---
	if (HandleKeyDown(SDL_SCANCODE_PAGEUP))
	{
		if (st->selectedElement == HUD_EDITOR_TEAM_BASE + 2)
			st->teamBarSize[1] += 64;
		else if (st->selectedElement < HUD_EDITOR_MAX_ELEMENTS)
			st->offsets[ep][st->selectedElement][3] += 64;
		st->modified = 1;
	}
	if (HandleKeyDown(SDL_SCANCODE_PAGEDOWN))
	{
		if (st->selectedElement == HUD_EDITOR_TEAM_BASE + 2)
			st->teamBarSize[1] -= 64;
		else if (st->selectedElement < HUD_EDITOR_MAX_ELEMENTS)
			st->offsets[ep][st->selectedElement][3] -= 64;
		st->modified = 1;
	}

	HandleKeyTapUpdate();
}

static void DrawCrosshair(u_long *ot, int ex, int ey, int selected)
{
	RECT r;
	if (selected)
	{
		r.x = ex - 3;
		r.y = ey - 3;
		r.w = 6;
		r.h = 6;
		CTR_Box_DrawClearBox(&r, &sdata->menuRowHighlight_Normal, TRANS_50_DECAL, ot);
		r.x = ex - 1;
		r.y = ey - 1;
		r.w = 2;
		r.h = 2;
		CTR_Box_DrawClearBox(&r, &MakeColor(0xff, 0xff, 0xff), TRANS_50_DECAL, ot);
	}
	else
	{
		r.x = ex - 1;
		r.y = ey - 1;
		r.w = 2;
		r.h = 2;
		CTR_Box_DrawClearBox(&r, &MakeColor(0x80, 0x80, 0x80), TRANS_50_DECAL, ot);
	}
}

void HudEditor_Render(void)
{
	struct GameTracker *gGT = sdata->gGT;
	u_long *ot = &gGT->backBuffer->otMem.startPlusFour[3];
	struct HudEditorState *st = &s_hudEditor;
	char buf[128];
	RECT r;
	int i;
	int ep = st->editPlayer;

	if (!st->active || !st->visible)
		return;

	if (st->helpPage)
	{
		r.x = 0x40;
		r.y = 0x10;
		r.w = 0x180;
		r.h = 0xD8;
		CTR_Box_DrawClearBox(&r, &MakeColor(0x00, 0x00, 0x00), TRANS_50_DECAL, ot);
		r.x += 1; r.y += 1; r.w -= 2; r.h -= 2;
		CTR_Box_DrawClearBox(&r, &MakeColor(0x18, 0x18, 0x20), TRANS_50_DECAL, ot);

		int hx = 0x58;
		int hy = 0x1A;
		DecalFont_DrawLineOT("HUD LAYOUT EDITOR", hx + 0xA0, hy, FONT_BIG, JUSTIFY_CENTER | ORANGE, ot);
		hy += 18;
		DecalFont_DrawLineOT("NAVIGATION", hx + 0xA0, hy, FONT_SMALL, JUSTIFY_CENTER | (ORANGE + 1), ot); hy += 12;
		DecalFont_DrawLineOT("Tab .............. Toggle Edit/Drive mode", hx, hy, FONT_SMALL, WHITE, ot); hy += 10;
		DecalFont_DrawLineOT("Q / R ............ Prev / Next element", hx, hy, FONT_SMALL, WHITE, ot); hy += 10;
		DecalFont_DrawLineOT("P ................. Cycle player (P1/P2/P3/P4)", hx, hy, FONT_SMALL, WHITE, ot); hy += 14;
		DecalFont_DrawLineOT("MOVEMENT", hx + 0xA0, hy, FONT_SMALL, JUSTIFY_CENTER | (ORANGE + 1), ot); hy += 12;
		DecalFont_DrawLineOT("Arrows ........... Move element 1px", hx, hy, FONT_SMALL, WHITE, ot); hy += 10;
		DecalFont_DrawLineOT("Shift+Arrows ..... Move element 4px", hx, hy, FONT_SMALL, WHITE, ot); hy += 10;
		DecalFont_DrawLineOT("Ctrl+Arrows ...... Move element 16px", hx, hy, FONT_SMALL, WHITE, ot); hy += 10;
		DecalFont_DrawLineOT("PageUp/PageDn .... Change scale", hx, hy, FONT_SMALL, WHITE, ot); hy += 14;
		DecalFont_DrawLineOT("EDITING", hx + 0xA0, hy, FONT_SMALL, JUSTIFY_CENTER | (ORANGE + 1), ot); hy += 12;
		DecalFont_DrawLineOT("N ................. Toggle element visibility", hx, hy, FONT_SMALL, WHITE, ot); hy += 10;
		DecalFont_DrawLineOT("Z ................. Reset element to default", hx, hy, FONT_SMALL, WHITE, ot); hy += 10;
		DecalFont_DrawLineOT("F ................. Reset ALL for current player", hx, hy, FONT_SMALL, WHITE, ot); hy += 10;
		DecalFont_DrawLineOT("Shift+C .......... Copy element to all players", hx, hy, FONT_SMALL, WHITE, ot); hy += 14;
		DecalFont_DrawLineOT("OPTIONS", hx + 0xA0, hy, FONT_SMALL, JUSTIFY_CENTER | (ORANGE + 1), ot); hy += 12;
		DecalFont_DrawLineOT("1/2/3/4 .......... Set player count + reload", hx, hy, FONT_SMALL, WHITE, ot); hy += 10;
		DecalFont_DrawLineOT("G ................. Cycle gamemode", hx, hy, FONT_SMALL, WHITE, ot); hy += 10;
		DecalFont_DrawLineOT("X ................. Toggle TEAM RACE mode", hx, hy, FONT_SMALL, WHITE, ot); hy += 10;
		DecalFont_DrawLineOT("C ................. Save positions", hx, hy, FONT_SMALL, WHITE, ot); hy += 10;
		DecalFont_DrawLineOT("V ................. Load positions", hx, hy, FONT_SMALL, WHITE, ot); hy += 10;
		DecalFont_DrawLineOT("H ................. Toggle this help", hx, hy, FONT_SMALL, WHITE, ot); hy += 10;
		DecalFont_DrawLineOT("Escape ........... Close editor", hx, hy, FONT_SMALL, WHITE, ot); hy += 14;
		DecalFont_DrawLineOT("Press H to close", hx + 0xA0, hy, FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
		return;
	}

	if (st->editMode)
	{
		for (i = 0; i < HUD_EDITOR_TOTAL_ELEMENTS; i++)
		{
			s16 ex, ey;

			if (i < HUD_EDITOR_MAX_ELEMENTS && st->hidden[ep][i])
				continue;

			if (i >= HUD_EDITOR_TEAM_BASE)
			{
				int t = i - HUD_EDITOR_TEAM_BASE;
				ex = st->offsets[ep][1][0];
				ey = st->offsets[ep][1][1];
				if (t == 0) { ex += st->teamIconOffset[0]; ey += st->teamIconOffset[1]; }
				else if (t == 1) { ex += st->teamTextOffset[0]; ey += st->teamTextOffset[1]; }
				else if (t == 2) { ex += st->teamBarOffset[0]; ey += st->teamBarOffset[1]; }
			}
			else
			{
				ex = st->offsets[ep][i][0];
				ey = st->offsets[ep][i][1];
				if (i == 9) { ex += 65; ey += 41; }
			}

			DrawCrosshair(ot, ex, ey, i == st->selectedElement);
		}
	}

	r.x = 0;
	r.y = 0;
	r.w = 0x200;
	r.h = 14;
	CTR_Box_DrawClearBox(&r, &MakeColor(0x00, 0x00, 0x00), TRANS_50_DECAL, ot);

	const char *elemName = (st->selectedElement >= 0 && st->selectedElement < HUD_EDITOR_TOTAL_ELEMENTS)
		? s_elementNames[st->selectedElement] : "???";
	s16 ex, ey, ez, es;

	if (st->selectedElement >= HUD_EDITOR_TEAM_BASE)
	{
		int t = st->selectedElement - HUD_EDITOR_TEAM_BASE;
		ex = st->offsets[ep][1][0];
		ey = st->offsets[ep][1][1];
		if (t == 0) { ex += st->teamIconOffset[0]; ey += st->teamIconOffset[1]; }
		else if (t == 1) { ex += st->teamTextOffset[0]; ey += st->teamTextOffset[1]; }
		else if (t == 2) { ex += st->teamBarOffset[0]; ey += st->teamBarOffset[1]; }
		ez = 0;
		es = (t == 2) ? st->teamBarSize[1] : 0;
	}
	else
	{
		ex = st->offsets[ep][st->selectedElement][0];
		ey = st->offsets[ep][st->selectedElement][1];
		ez = st->offsets[ep][st->selectedElement][2];
		es = st->offsets[ep][st->selectedElement][3];
	}

	const char *hidden = (st->selectedElement < HUD_EDITOR_MAX_ELEMENTS && st->hidden[ep][st->selectedElement]) ? " HIDDEN" : "";

	snprintf(buf, sizeof(buf),
		"[%s] %s P%d | %s%s | [%02d] %s  x:%d y:%d z:%d s:%d",
		st->editMode ? "EDIT" : "DRIVE",
		s_playerCountNames[st->playerCount],
		ep + 1,
		s_gamemodeNames[st->gamemodeIndex],
		st->modified ? " *" : "",
		st->selectedElement, elemName,
		(int)ex, (int)ey, (int)ez, (int)es);
	DecalFont_DrawLineOT((char *)buf, 4, 3, FONT_SMALL, WHITE, ot);

	r.x = 0;
	r.y = 0xF2;
	r.w = 0x200;
	r.h = 0x0E;
	CTR_Box_DrawClearBox(&r, &MakeColor(0x00, 0x00, 0x00), TRANS_50_DECAL, ot);

	if (st->editMode)
	{
		DecalFont_DrawLineOT(
			"Tab:Drive  P:Player  Q/R:Sel  N:Hide  Z:Reset  F:ResetAll  H:Help  Esc:Close",
			0x100, 0xF4, FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
	}
	else
	{
		DecalFont_DrawLineOT(
			"Tab:Edit   Drive the kart freely - HUD positions visible",
			0x100, 0xF4, FONT_SMALL, JUSTIFY_CENTER | ORANGE, ot);
	}
}

void HudEditor_Save(void)
{
	struct HudEditorState *st = &s_hudEditor;
	const char *base = NativeAssets_GetBaseDir();
	char path[256];
	FILE *f;
	int i, p;

	if (base == NULL)
		base = ".";
	snprintf(path, sizeof(path), "%s/hud_layout.txt", base);

	f = fopen(path, "w");
	if (f == NULL)
		return;

	fprintf(f, "// CTR HUD Layout - Generated by HUD Layout Editor\n");
	fprintf(f, "// Player Count: %d\n", st->playerCount);
	fprintf(f, "// Team Race Mode: %s\n", st->teamRaceMode ? "ON" : "OFF");
	fprintf(f, "//\n");

	for (p = 0; p < st->playerCount; p++)
	{
		fprintf(f, "// === Player %d ===\n", p + 1);
		for (i = 0; i < HUD_EDITOR_MAX_ELEMENTS; i++)
		{
			fprintf(f, "// P%d 0x%02X %-16s x=%-5d y=%-5d z=%-5d scale=%-5d hidden=%d\n",
				p + 1, i, s_elementNames[i],
				(int)st->offsets[p][i][0], (int)st->offsets[p][i][1],
				(int)st->offsets[p][i][2], (int)st->offsets[p][i][3],
				(int)st->hidden[p][i]);
		}
	}

	fprintf(f, "//\n");
	fprintf(f, "// Team Race Offsets:\n");
	fprintf(f, "// Icon Offset:  x=%-5d y=%-5d\n", (int)st->teamIconOffset[0], (int)st->teamIconOffset[1]);
	fprintf(f, "// Text Offset:  x=%-5d y=%-5d\n", (int)st->teamTextOffset[0], (int)st->teamTextOffset[1]);
	fprintf(f, "// Bar Offset:   x=%-5d y=%-5d\n", (int)st->teamBarOffset[0], (int)st->teamBarOffset[1]);
	fprintf(f, "// Bar Size:     w=%-5d h=%-5d\n", (int)st->teamBarSize[0], (int)st->teamBarSize[1]);

	fclose(f);
	st->modified = 0;
}

void HudEditor_Load(void)
{
	struct HudEditorState *st = &s_hudEditor;
	const char *base = NativeAssets_GetBaseDir();
	char path[256];
	FILE *f;
	char line[256];

	if (base == NULL)
		base = ".";
	snprintf(path, sizeof(path), "%s/hud_layout.txt", base);

	f = fopen(path, "r");
	if (f == NULL)
		return;

	while (fgets(line, sizeof(line), f))
	{
		if (line[0] != '/' || line[1] != '/')
			continue;

		int p, idx, x, y, z, sc, hd;

		if (sscanf(line + 2, " P%d 0x%02X %*s x=%d y=%d z=%d scale=%d hidden=%d", &p, &idx, &x, &y, &z, &sc, &hd) == 7)
		{
			if (p >= 1 && p <= 4 && idx >= 0 && idx < HUD_EDITOR_MAX_ELEMENTS)
			{
				st->offsets[p - 1][idx][0] = (s16)x;
				st->offsets[p - 1][idx][1] = (s16)y;
				st->offsets[p - 1][idx][2] = (s16)z;
				st->offsets[p - 1][idx][3] = (s16)sc;
				st->hidden[p - 1][idx] = (Uint8)hd;
			}
		}
		else if (sscanf(line + 2, " P%d 0x%02X %*s x=%d y=%d z=%d scale=%d", &p, &idx, &x, &y, &z, &sc) == 6)
		{
			if (p >= 1 && p <= 4 && idx >= 0 && idx < HUD_EDITOR_MAX_ELEMENTS)
			{
				st->offsets[p - 1][idx][0] = (s16)x;
				st->offsets[p - 1][idx][1] = (s16)y;
				st->offsets[p - 1][idx][2] = (s16)z;
				st->offsets[p - 1][idx][3] = (s16)sc;
			}
		}
		else if (sscanf(line + 2, " 0x%02X %*s x=%d y=%d z=%d scale=%d", &idx, &x, &y, &z, &sc) == 5)
		{
			if (idx >= 0 && idx < HUD_EDITOR_MAX_ELEMENTS)
			{
				st->offsets[0][idx][0] = (s16)x;
				st->offsets[0][idx][1] = (s16)y;
				st->offsets[0][idx][2] = (s16)z;
				st->offsets[0][idx][3] = (s16)sc;
			}
		}
		else if (strstr(line, "Icon Offset:"))
		{
			sscanf(line, "// Icon Offset:  x=%d y=%d", &x, &y);
			st->teamIconOffset[0] = (s16)x;
			st->teamIconOffset[1] = (s16)y;
		}
		else if (strstr(line, "Text Offset:"))
		{
			sscanf(line, "// Text Offset:  x=%d y=%d", &x, &y);
			st->teamTextOffset[0] = (s16)x;
			st->teamTextOffset[1] = (s16)y;
		}
		else if (strstr(line, "Bar Offset:"))
		{
			sscanf(line, "// Bar Offset:   x=%d y=%d", &x, &y);
			st->teamBarOffset[0] = (s16)x;
			st->teamBarOffset[1] = (s16)y;
		}
		else if (strstr(line, "Bar Size:"))
		{
			sscanf(line, "// Bar Size:     w=%d h=%d", &x, &y);
			st->teamBarSize[0] = (s16)x;
			st->teamBarSize[1] = (s16)y;
		}
	}

	fclose(f);
}

#endif
