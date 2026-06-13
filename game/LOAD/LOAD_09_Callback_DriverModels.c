#include <common.h>

/* Debug macros for the bigfile loading path (defined in native_bigfile.h) */

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80031b00-0x80031b14.
void LOAD_Callback_DriverModels(struct LoadQueueSlot *lqs)
{
        BFDBG_PRINTF("Callback_DriverModels: load_inProgress -> 0, ptrMPK=%p",
                lqs->ptrDestination);
        sdata->load_inProgress = 0;
        sdata->ptrMPK = (int)lqs->ptrDestination;
}
