#include <common.h>

enum CrystalChallengeEndMenuConstants
{
	CC_FIRST_PURPLE_TOKEN_BIT = 0x6f,
	CC_FLY_IN_FRAMES = 0x14,
	CC_SCREEN_DEPTH = 0x200,
	CC_TOKEN_GROW_LIMIT = 0x2001,
	CC_TOKEN_GROW_STEP = 0x200,
};

global_variable const s16 s_battleTrackRewardOffset[LAB_BASEMENT - NITRO_COURT + 1] = {
    [NITRO_COURT - NITRO_COURT] = 3,     // Citadel City
    [RAMPAGE_RUINS - NITRO_COURT] = 1,   // Lost Ruins
    [PARKING_LOT - NITRO_COURT] = -1,    // not used in any hub
    [SKULL_ROCK - NITRO_COURT] = 0,      // N. Sanity Beach
    [THE_NORTH_BOWL - NITRO_COURT] = -1, // not used in any hub
    [ROCKY_ROAD - NITRO_COURT] = 2,      // Glacier Park
    [LAB_BASEMENT - NITRO_COURT] = -1,   // not used in any hub
};

extern struct RectMenu menu221;

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8009f710-0x8009fbec.
void CC_EndEvent_DrawMenu()
{
	struct GameTracker *gGT = sdata->gGT;
	s32 levelID = gGT->levelID;
	struct Driver *driver = gGT->drivers[0];
	s16 posXY[2];
	s32 tokenRewardOffset;

	// "Dingo Bingo" $sp exploit, for 101% speedruns.
	// Dingo Canyon gives different item depending on
	// camera, Blizz Bluff gives Skull Rock token, and
	// Dragon Mines gives purple gem
	if (levelID == DINGO_CANYON)
		tokenRewardOffset = gGT->pushBuffer[0].pos[2];
	else if (levelID == DRAGON_MINES)
		tokenRewardOffset = 0;
	else if (levelID == BLIZZARD_BLUFF)
		tokenRewardOffset = -1;

	// default logic
	else
		tokenRewardOffset = s_battleTrackRewardOffset[levelID - NITRO_COURT];

	s32 tokenRewardBit = tokenRewardOffset + CC_FIRST_PURPLE_TOKEN_BIT;
	struct AdvProgress *adv = &sdata->advProgress;
	b32 didLose = driver->numCrystals < gGT->numCrystalsInLEV;
	s32 elapsedFrames = sdata->framesSinceRaceEnded;

	if (elapsedFrames < CTR_SECONDS_TO_FRAMES(30))
		elapsedFrames++;

	sdata->framesSinceRaceEnded = elapsedFrames;
	sdata->ptrHudCrystal->flags |= HIDE_MODEL;

	// fly in from left
	UI_Lerp2D_Linear(&posXY[0], -0x64, 0x18, 0x100, 0x18, elapsedFrames, CC_FLY_IN_FRAMES);
	DecalFont_DrawLine(sdata->lngStrings[LNG_TIME_REMAINING], posXY[0], posXY[1], FONT_BIG, (JUSTIFY_CENTER | ORANGE));
	UI_DrawLimitClock(posXY[0] - 0x33, posXY[1] + 0x11, FONT_BIG);

	// fly in from right
	UI_Lerp2D_Linear(&posXY[0], 0x264, 0x56, 0xcd, 0x56, elapsedFrames, CC_FLY_IN_FRAMES);

	// Crystal count
	sdata->ptrMenuCrystal->matrix.t[0] = UI_ConvertX_2(posXY[0], CC_SCREEN_DEPTH);
	sdata->ptrMenuCrystal->matrix.t[1] = UI_ConvertY_2(posXY[1], CC_SCREEN_DEPTH);
	UI_DrawNumCrystal(posXY[0] + 0xf, posXY[1] - 0x10, driver);

	s32 resultStringIndex = LNG_YOU_WIN;
	if (didLose)
		resultStringIndex = LNG_TRY_AGAIN;

	// YOU WIN, or TRY AGAIN
	DecalFont_DrawLine(sdata->lngStrings[resultStringIndex], posXY[0] + 0x33, posXY[1] + 8, FONT_BIG, (JUSTIFY_CENTER | ORANGE));

	// if a token is not newly-unlocked
	if (didLose || (CHECK_ADV_BIT(adv->rewards, tokenRewardBit) != 0))
	{
		// If you pressed X/O to continue, quit function
		if ((sdata->menuReadyToPass & 1) != 0)
			return;

		DecalFont_DrawLine(sdata->lngStrings[LNG_PRESS_TO_CONTINUE], 0x100, 0xbe, FONT_BIG, (JUSTIFY_CENTER | ORANGE));

		if ((sdata->AnyPlayerTap & (BTN_CROSS | BTN_CIRCLE)) == 0)
			return;

		RECTMENU_ClearInput();
		RECTMENU_Show(&menu221); // Retry / Exit To Map menu
		sdata->menuReadyToPass |= 1;
		return;
	}

	// == if a token is newly-unlocked ==

	struct Instance *token = sdata->ptrToken;
	s32 color = (JUSTIFY_CENTER | ORANGE);
	if (gGT->timer == 0)
		color = (JUSTIFY_CENTER | WHITE);

	UI_Lerp2D_Linear(&posXY[0], -0x64, 0xA2, 0x100, 0xA2, elapsedFrames, CC_FLY_IN_FRAMES);

	DecalFont_DrawLine(sdata->lngStrings[LNG_CTR_TOKEN_AWARDED], posXY[0], posXY[1], FONT_BIG, color);
	token->flags &= ~(HIDE_MODEL);
	token->matrix.t[0] = UI_ConvertX_2(posXY[0], CC_SCREEN_DEPTH);
	token->matrix.t[1] = UI_ConvertY_2(0xA2 - 0x18, CC_SCREEN_DEPTH);

	if (elapsedFrames > CTR_SECONDS_TO_FRAMES(1))
	{
		if (token->scale[0] < CC_TOKEN_GROW_LIMIT)
		{
			token->scale[0] += CC_TOKEN_GROW_STEP;
			token->scale[1] += CC_TOKEN_GROW_STEP;
			token->scale[2] += CC_TOKEN_GROW_STEP;
		}
	}
	else if (elapsedFrames == CTR_SECONDS_TO_FRAMES(1))
	{
		// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8009fa24-0x8009fa2c for crystal token unlock SFX.
		OtherFX_Play(0x67, 1);
	}

	DecalFont_DrawLine(sdata->lngStrings[LNG_PRESS_TO_CONTINUE], 0x100, 0xbe, FONT_BIG, (JUSTIFY_CENTER | ORANGE));

	// if still waiting to press X/O, quit function
	if ((sdata->AnyPlayerTap & (BTN_CROSS | BTN_CIRCLE)) == 0)
		return;

	// if pressed X/O,
	// unlock token and leave level

	RECTMENU_ClearInput();
	sdata->framesSinceRaceEnded = 0;

	sdata->Loading.OnBegin.AddBitsConfig0 |= ADVENTURE_ARENA;
	sdata->Loading.OnBegin.RemBitsConfig0 |= CRYSTAL_CHALLENGE;
	UNLOCK_ADV_BIT(adv->rewards, tokenRewardBit);
	MainRaceTrack_RequestLoad(gGT->prevLEV); // NOTE(aalhendi): Adv hub.

	return;
}

struct MenuRow rows221[3] = {
    // Retry
    {
        .stringIndex = LNG_RETRY,
        .rowOnPressUp = 0,
        .rowOnPressDown = 1,
        .rowOnPressLeft = 0,
        .rowOnPressRight = 0,
    },

    // Exit to map
    {
        .stringIndex = LNG_EXIT_TO_MAP,
        .rowOnPressUp = 0,
        .rowOnPressDown = 1,
        .rowOnPressLeft = 1,
        .rowOnPressRight = 1,
    },

    // NULL, end of menu
    {
        .stringIndex = 0xFFFF,
        .rowOnPressUp = 0,
        .rowOnPressDown = 0,
        .rowOnPressLeft = 0,
        .rowOnPressRight = 0,
    }};

struct RectMenu menu221 = {
    .stringIndexTitle = 0xFFFF,
    .posX_curr = 0x100,
    .posY_curr = 0xB4,

    .unk1 = 0,

    .state = 0x803,
    .rows = rows221,
    .funcPtr = UI_RaceEnd_MenuProc,
    .drawStyle = 4,

    // rest of variables all default zero
};
