#include <common.h>

// NOTE(aalhendi): Modified to skip past locked characters by following the indexNext chain.
int MM_Characters_GetNextDriver(s16 dpad, char characterID)
{
	char nextDriver;
	s16 unlocked;
	char newDriver;

	nextDriver = D230.csm_Active[characterID].indexNext[dpad];
	unlocked = D230.csm_Active[nextDriver].unlockFlags;

	// set new driver to the driver
	// you'd get when pressing Up button
	newDriver = nextDriver;

	if (
	    // if desired driver is not unlocked by default
	    (unlocked != -1) &&

	    (((sdata->gameProgress.unlocks[unlocked >> 5] >> (unlocked & 0x1f)) & 1) == 0))
	{
		// Walk the indexNext chain to find the next unlocked character
		char prev = nextDriver;
		char curr = D230.csm_Active[prev].indexNext[dpad];
		while (curr != prev)
		{
			unlocked = D230.csm_Active[curr].unlockFlags;
			if (unlocked == -1 || ((sdata->gameProgress.unlocks[unlocked >> 5] >> (unlocked & 0x1f)) & 1) != 0)
			{
				newDriver = curr;
				break;
			}
			prev = curr;
			curr = D230.csm_Active[curr].indexNext[dpad];
		}
		// If no unlocked character found in the chain, stay put
		if (newDriver == nextDriver)
			newDriver = characterID;
	}

	// return new driver
	return newDriver;
}
