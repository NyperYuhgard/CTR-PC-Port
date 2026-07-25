#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800327dc-0x8003282c.
void LOAD_Robots1P(int characterID)
{
	int newCharacterID = 0;

	data.characterIDs[0] = characterID;

#ifdef CTR_NATIVE
	struct GameTracker *gGT = sdata->gGT;
	if ((gGT->gameMode2 & TEAM_RACE_MODE) != 0)
	{
		// Team Race: random teammate from 8 main characters (0-7)
		// Main 8: Crash(0), Cortex(1), Tiny(2), Coco(3), N.Gin(4), Dingodile(5), Polar(6), Pura(7)

		// Build array of available main characters (excluding player's choice)
		int availableMainChars[8];
		int availCount = 0;
		for (int i = 0; i < 8; i++)
		{
			if (i != characterID)
				availableMainChars[availCount++] = i;
		}

		// Fisher-Yates shuffle for random teammate selection
		for (int i = availCount - 1; i > 0; i--)
		{
			int j = MixRNG_Scramble() % (i + 1);
			int tmp = availableMainChars[i];
			availableMainChars[i] = availableMainChars[j];
			availableMainChars[j] = tmp;
		}

		// Assign random teammate from main 8
		data.characterIDs[1] = availableMainChars[0];

		// Assign remaining 6 AI from remaining main 8 (excluding player + teammate)
		int remainingMainChars[6];
		int remCount = 0;
		for (int i = 0; i < 8; i++)
		{
			if (i != characterID && i != data.characterIDs[1])
				remainingMainChars[remCount++] = i;
		}

		// Shuffle remaining
		for (int i = remCount - 1; i > 0; i--)
		{
			int j = MixRNG_Scramble() % (i + 1);
			int tmp = remainingMainChars[i];
			remainingMainChars[i] = remainingMainChars[j];
			remainingMainChars[j] = tmp;
		}

		for (int i = 2; i < 8; i++)
		{
			data.characterIDs[i] = remainingMainChars[i - 2];
		}
	}
	else
#endif
	{
		for (int i = 1; i < 8; i++, newCharacterID++)
		{
			if (newCharacterID == characterID)
				newCharacterID++;

#ifdef CTR_NATIVE
			// Cooperative adventure: skip overwriting P2's character
			if ((gGT->gameMode2 & COOPERATIVE_ADVENTURE) != 0 && i == 1)
				continue;
#endif

			data.characterIDs[i] = newCharacterID;
		}
	}
}
