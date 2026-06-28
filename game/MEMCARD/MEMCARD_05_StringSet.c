#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003d730-0x8003d7d8.
void MEMCARD_StringSet(char *dstString, int slotIdx, char *srcString)

{
	int i;
	MEMCARD_StringInit(slotIdx, dstString);

	// fast strlen
	for (i = 0; dstString[i] != '\0'; i++)
		;

	// copy string from src to dst
	for (int j = 0; (srcString[j] != '\0' && i < 63); j++)
	{
		dstString[i] = srcString[j];
		i++;
	}

	// nullptr
	dstString[i] = '\0';
	return;
}
