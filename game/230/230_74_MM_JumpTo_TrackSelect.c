#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b4430-0x800b446c.
void MM_JumpTo_TrackSelect(void)
{
	// return to track selection
	sdata->ptrActiveMenu = &D230.menuTrackSelect;

	D230.menuTrackSelect.state &= ~(ONLY_DRAW_TITLE);

	MM_TrackSelect_Init();
}
