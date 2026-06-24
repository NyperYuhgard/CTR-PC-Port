#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80056220-0x800562fc.
void UI_CupStandings_UpdateCupRanks(void)
{
	struct GameTracker *gGT;
	int bestScore;
	int bestIndex;
	int assignedMask;
	int numDrivers;

	bestScore = 0;
	bestIndex = -1;
	gGT = sdata->gGT;
	assignedMask = 0;

	numDrivers = (u8)gGT->numPlyrCurrGame + (u8)gGT->numBotsNextGame;
	if (numDrivers == 0)
		return;

	for (int rankSlot = 0; rankSlot < numDrivers; rankSlot++)
	{
		for (int driverIndex = numDrivers - 1; driverIndex >= 0; driverIndex--)
		{
			if ((gGT->cup.points[driverIndex] >= (s16)bestScore) && (((assignedMask >> driverIndex) & 1) == 0))
			{
				bestScore = (u16)gGT->cup.points[driverIndex];

				if ((s16)bestIndex != -1)
					assignedMask &= ~(1 << bestIndex);

				bestIndex = driverIndex;
				assignedMask |= 1 << driverIndex;
			}
		}

		data.cupPositionPerPlayer[rankSlot] = (s16)bestIndex;
		bestScore = 0;
		bestIndex = -1;
	}
}
