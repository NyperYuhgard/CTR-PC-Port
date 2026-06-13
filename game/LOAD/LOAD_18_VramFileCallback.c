#include <common.h>

/* Debug macros for the bigfile loading path (defined in native_bigfile.h) */

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80031ee4-0x80031fdc.
void LOAD_VramFileCallback(struct LoadQueueSlot *lqs)
{
        int *vramBuf = lqs->ptrDestination;

        struct VramHeader *vh = (struct VramHeader *)vramBuf;

        BFDBG_PRINTF("VramFileCallback: subfile=%d, ptrDst=%p, userCallback=%p",
                lqs->subfileIndex, lqs->ptrDestination, (void *)lqs->callbackFuncPtr);

        // if just one TIM
        if ((vramBuf != NULL) && (vramBuf[0] != 0x20))
        {
                BFDBG_PRINTF("VramFileCallback: subfile=%d, single TIM upload", lqs->subfileIndex);
                LoadImage(&vh->rect, VRAMHEADER_GETPIXLES(vh));
        }

        // if multiple TIMs are packed together
        if ((vramBuf != NULL) && (vramBuf[0] == 0x20))
        {
                int size;
                vramBuf++;

                size = vramBuf[0];
                vh = (struct VramHeader *)&vramBuf[1];

                BFDBG_PRINTF("VramFileCallback: subfile=%d, multi-TIM upload", lqs->subfileIndex);

                while (size != 0)
                {
                        LoadImage(&vh->rect, VRAMHEADER_GETPIXLES(vh));

                        // goto next
                        vramBuf = (u32)vh + size;

                        size = vramBuf[0];
                        vh = (struct VramHeader *)&vramBuf[1];
                }
        }

        // LOAD_NextQueuedFile waits 3 vsync frames before releasing the queue.
        sdata->frameFinishedVRAM = sdata->gGT->frameTimer_VsyncCallback;
        BFDBG_PRINTF("VramFileCallback: subfile=%d, frameFinishedVRAM set — waiting 3 vsync frames",
                lqs->subfileIndex);
}
