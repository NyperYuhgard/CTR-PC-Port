#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80032d30-0x80032d8c.
void LOAD_AppendQueue(struct BigHeader *bigfile, int type, int fileIndex, void *destinationPtr, void (*callback)(struct LoadQueueSlot *))
{
	struct LoadQueueSlot *lqs;

	if (sdata->queueLength >= 8)
		return;

	lqs = &sdata->queueSlots[sdata->queueLength];
	lqs->ptrBigfileCdPos_UNUSED = bigfile;
	lqs->flags = 0;
	lqs->type_UNUSED = type;
	lqs->subfileIndex = fileIndex;
	lqs->ptrDestination = destinationPtr;
	lqs->size_UNUSED = 0;
	lqs->callbackFuncPtr = callback;

	sdata->queueLength++;
}
