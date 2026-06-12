#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003db98-0x8003dbf8.
int MEMCARD_NewTask(int slotIdx, char *name, u8 *ptrMemcard, int memcardFileSize, int unused)
{
	sdata->memcardSlot = slotIdx;

	MEMCARD_StringSet(sdata->s_memcardFileCurr, slotIdx, name);

	sdata->memcard_ptrStart = ptrMemcard;
	sdata->memcard_remainingAttempts = 8;
	sdata->memcardFileSize = memcardFileSize;

	return 0;
}
