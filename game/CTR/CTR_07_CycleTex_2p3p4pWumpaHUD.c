#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80021b94-0x80021bbc.
void CTR_CycleTex_2p3p4pWumpaHUD(u32 *ptrActiveTex, u32 *ptrArray, int numFrames)
{
	ptrArray[0] = ptrActiveTex[0];
	ptrActiveTex[0] = (u32)((uintptr_t)&ptrArray[numFrames - 1] & 0x00ffffff);
}
