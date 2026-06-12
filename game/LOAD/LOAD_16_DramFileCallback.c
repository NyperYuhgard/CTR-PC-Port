#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 PS1 path 0x80031d30-0x80031e00.
void LOAD_DramFileCallback(struct LoadQueueSlot *lqs)
{
	char *fileBuf = lqs->ptrDestination;
	void (*callback)(struct LoadQueueSlot *) = lqs->callbackFuncPtr;

	if (fileBuf != NULL)
	{
		int ptrMapOffset = *(int *)&fileBuf[0];
		char *realFileBuf = &fileBuf[4];

		if (ptrMapOffset >= 0)
		{
			struct DramPointerMap *dpm = (struct DramPointerMap *)&realFileBuf[ptrMapOffset];

			LOAD_RunPtrMap(realFileBuf, (int *)DRAM_GETOFFSETS(dpm), dpm->numBytes >> 2);

#if defined(CTR_NATIVE)
			if ((lqs->flags & LT_MEMPACK) != 0)
#else
			if ((lqs->flags & LT_SETADDR) != 0)
#endif
			{
				MEMPACK_ReallocMem(ptrMapOffset + 4);
			}
		}
		else
		{
			lqs->flags |= LT_GETADDR;
		}

		lqs->ptrDestination = &fileBuf[4];
	}

#if defined(CTR_NATIVE)
	// NOTE(aalhendi): CTR_NATIVE keeps host callback pointers and queue sentinels.
	if ((callback != NULL) && (callback != LOAD_DramFileCallback) && (callback != (void (*)(struct LoadQueueSlot *))-1) &&
	    (callback != (void (*)(struct LoadQueueSlot *))-2))
#else
	if ((callback != NULL) && (((u32)(uintptr_t)callback & 0xff000000) == 0x80000000))
#endif
	{
		callback(lqs);
	}

	sdata->queueReady = 1;
}
