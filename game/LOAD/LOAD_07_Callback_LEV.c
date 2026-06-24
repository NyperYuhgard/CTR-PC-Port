#include <common.h>

/* Debug macros for the bigfile loading path (defined in native_bigfile.h) */

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80031a78-0x80031aa4.
void LOAD_Callback_LEV(struct LoadQueueSlot *lqs)
{
        BFDBG_PRINTF("Callback_LEV: flags=0x%X, ptrDst=%p, load_inProgress -> %d",
                lqs->flags, lqs->ptrDestination, (lqs->flags & LT_GETADDR) ? 1 : 0);

        if ((lqs->flags & LT_GETADDR) == 0)
                sdata->load_inProgress = 0;

        sdata->ptrLevelFile = (struct Level *)lqs->ptrDestination;
}
