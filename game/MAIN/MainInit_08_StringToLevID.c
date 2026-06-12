#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003c1d4-0x8003c248.
int MainInit_StringToLevID(char *str)
{
	for (int levelID = 0; levelID < 0x41; levelID++)
	{
		char *debugName = data.metaDataLEV[levelID].name_Debug;

		if (strncmp(debugName, str, strlen(debugName)) == 0)
			return levelID;
	}

	return 0;
}
