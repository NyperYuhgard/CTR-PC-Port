#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80031fdc-0x80032110.
void *LOAD_VramFile(void *bigfilePtr, int subfileIndex, void *ptrDestination, int *sizePtr, int callbackOrFlags)
{
	struct LoadQueueSlot lqs;
	void *loadedFile;

	if (ptrDestination == NULL)
		MEMPACK_PushState();

	if (callbackOrFlags == -1)
	{
		loadedFile = LOAD_ReadFile_ex(bigfilePtr, LT_VRAM, subfileIndex, ptrDestination, sizePtr, NULL);

		lqs.ptrBigfileCdPos_UNUSED = bigfilePtr;
		lqs.flags = 0;
		lqs.type_UNUSED = LT_VRAM;
		lqs.subfileIndex = subfileIndex;
		lqs.ptrDestination = loadedFile;
		lqs.size_UNUSED = *sizePtr;
		lqs.callbackFuncPtr = NULL;

		LOAD_VramFileCallback(&lqs);

		VSync(2);
		sdata->frameFinishedVRAM = 0;

		if (ptrDestination == NULL)
			MEMPACK_PopState();

		return loadedFile;
	}

	if (callbackOrFlags == -2)
	{
		loadedFile = LOAD_ReadFile_ex(bigfilePtr, LT_VRAM, subfileIndex, NULL, sizePtr, LOAD_VramFileCallback);
		data.currSlot.ptrDestination = loadedFile;
		*(void **)ptrDestination = loadedFile;
		return loadedFile;
	}

	return LOAD_ReadFile_ex(bigfilePtr, LT_VRAM, subfileIndex, ptrDestination, sizePtr, LOAD_VramFileCallback);
}
