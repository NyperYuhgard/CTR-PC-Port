#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003d6e8-0x8003d730.
char *MEMCARD_StringInit(int slotIndex, char *dstString)
{
	if (!dstString)
	{
		dstString = &sdata->s_memcardDirHeader[0];
	}

	dstString[0] = 'b';
	dstString[1] = 'u';
	dstString[2] = '0' + ((slotIndex >> 4) & 1);
	dstString[3] = '0' + (slotIndex & 3);
	dstString[4] = ':';
	dstString[5] = '\0';
	return dstString;
}
