#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b3914-0x800b3954.
void MM_HighScore_Init(void)
{
	D230.highScore_transitionState = ENTERING_MENU;
	D230.highScore_transitionFrames[0] = 0xc;
	D230.highScore_rowDesired = 0;
	D230.highScore_rowCurr = 0;

	// reset all video variables
	MM_TrackSelect_Video_SetDefaults();
}
