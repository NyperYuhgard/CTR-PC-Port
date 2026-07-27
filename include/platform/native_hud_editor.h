#ifndef NATIVE_HUD_EDITOR_H
#define NATIVE_HUD_EDITOR_H

#ifdef CTR_NATIVE_DEV_HUD_EDITOR

#include <psx/types.h>

struct UiElement2D;

#define HUD_EDITOR_MAX_ELEMENTS 0x14
#define HUD_EDITOR_TEAM_BASE 0x14
#define HUD_EDITOR_TOTAL_ELEMENTS (HUD_EDITOR_TEAM_BASE + 3)

struct HudEditorState
{
	int active;
	int editMode; // 0 = drive kart, 1 = edit HUD positions
	int selectedElement;
	int editPlayer; // 0-3, which player's HUD is being edited
	int playerCount;
	int teamRaceMode;
	int gamemodeIndex; // 0=Arcade, 1=Battle, 2=TimeTrial, 3=Relic, 4=TeamRace
	s16 baseOffsets[4][HUD_EDITOR_MAX_ELEMENTS][4]; // originals from data tables
	s16 offsets[4][HUD_EDITOR_MAX_ELEMENTS][4]; // [player][element][x,y,z,scale]
	s16 hidden[4][HUD_EDITOR_MAX_ELEMENTS]; // per-player visibility: 0=visible, 1=hidden
	int modified;
	int visible;
	int helpPage;
	s16 teamIconOffset[2];
	s16 teamTextOffset[2];
	s16 teamBarOffset[2];
	s16 teamBarSize[2];
};

void HudEditor_Init(void);
void HudEditor_Activate(void);
void HudEditor_Deactivate(void);
int  HudEditor_IsActive(void);
void HudEditor_HandleInput(void);
void HudEditor_Render(void);
void HudEditor_Save(void);
void HudEditor_Load(void);
struct HudEditorState *HudEditor_GetState(void);
struct UiElement2D *HudEditor_GetOffsets(void);
void HudEditor_GetTeamOffsets(s16 *iconOff, s16 *textOff, s16 *barOff, s16 *barSize);

#endif
#endif
