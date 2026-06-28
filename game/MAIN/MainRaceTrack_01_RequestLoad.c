#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003cfc0-0x8003d024.
void MainRaceTrack_RequestLoad(s16 levelID)
{
	// Turn off HUD
	sdata->gGT->hudFlags &= 0xfe;

	if (RaceFlag_IsFullyOffScreen() == 1)
	{
		RaceFlag_BeginTransition(1);
	}
	RaceFlag_ResetTextAnim();

	sdata->Loading.stage = -4;
	sdata->Loading.Lev_ID_To_Load = levelID;
	return;
}
