#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800ae274-0x800ae2c0.
void MM_Characters_BackupIDs(void)
{
	char i;

	for (i = 0; i < 8; i++)
	{
		// make a backup when you leave character selection,
		// backup is restored when you go back to selection
		sdata->characterIDs_backup[i] = data.characterIDs[i];
	}
	return;
}
