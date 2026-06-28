#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8005607c-0x80056220.
void UI_CupStandings_FinalizeCupRanks(void)
{
	struct GameTracker *gGT;
	int numDrivers;
	int tiedTopCount;
	int topScore;
	int bestRank;
	int selectedRankSlot;

	bestRank = 99;
	gGT = sdata->gGT;
	selectedRankSlot = -1;

	numDrivers = (u8)gGT->numPlyrCurrGame + (u8)gGT->numBotsNextGame;
	if (numDrivers >= 5)
		numDrivers = 4;

	tiedTopCount = 0;
	if (1 < numDrivers)
	{
		topScore = gGT->cup.points[data.cupPositionPerPlayer[0]];

		for (int i = 1; i < numDrivers; i++)
		{
			if (gGT->cup.points[data.cupPositionPerPlayer[i]] != topScore)
				break;

			tiedTopCount++;
		}
	}

	for (int rankSlot = 0; rankSlot < tiedTopCount + 1; rankSlot++)
	{
		for (int i = rankSlot; i < tiedTopCount + 1; i++)
		{
			struct Driver *driver;
			driver = gGT->drivers[data.cupPositionPerPlayer[i]];

			if (driver->driverRank < (s16)bestRank)
			{
				bestRank = (u16)driver->driverRank;
				selectedRankSlot = i;
			}
		}

		int previousRankValue = (u16)data.cupPositionPerPlayer[rankSlot];
		data.cupPositionPerPlayer[rankSlot] = (s16)selectedRankSlot;
		data.cupPositionPerPlayer[selectedRankSlot] = (s16)previousRankValue;

		bestRank = 99;
	}
}
