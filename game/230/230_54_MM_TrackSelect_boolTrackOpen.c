#include <common.h>

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 230 0x800aff58-0x800affd0.
char MM_TrackSelect_boolTrackOpen(struct MainMenu_LevelRow *menuSelect)
{
	s16 flag = menuSelect->unlock;

	if (flag == -1)
		return true;

	if (flag == -2)
		return sdata->gGT->numPlyrNextGame == 1;

	if (flag < 0)
		return false;

	return (sdata->gameProgress.unlocks[flag >> 5] >> (flag & 0x1f)) & 1;
}
