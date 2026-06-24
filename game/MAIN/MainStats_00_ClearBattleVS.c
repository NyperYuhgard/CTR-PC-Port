#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003d024-0x8003d068.
void MainStats_ClearBattleVS(void)
{
	int i;
	struct GameTracker *gGT = sdata->gGT;

	// 4 players, 3 ranks (1st, 2nd, 3rd)
	for (i = 0; i < 12; i++)
	{
		gGT->standingsPoints[i] = 0;
	}

	return;
}
