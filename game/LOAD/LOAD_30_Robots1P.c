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
		data.characterIDs[1] = (characterID + 1) & 0xF;
		if (data.characterIDs[1] == characterID)
			data.characterIDs[1] = (characterID + 2) & 0xF;

		newCharacterID = 0;
		for (int i = 2; i < 8; i++, newCharacterID++)
		{
			if (newCharacterID == characterID)
				newCharacterID++;
			if (newCharacterID == data.characterIDs[1])
				newCharacterID++;
			data.characterIDs[i] = newCharacterID;
		}
	}
	else
#endif
	{
		for (int i = 1; i < 8; i++, newCharacterID++)
		{
			if (newCharacterID == characterID)
				newCharacterID++;

			data.characterIDs[i] = newCharacterID;
		}
	}
}
