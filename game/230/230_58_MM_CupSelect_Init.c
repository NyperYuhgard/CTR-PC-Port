#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b0eb8-0x800b0eec.
void MM_CupSelect_Init(void)
{
	// reset transition data
	D230.cupSel_transitionFrames = 0xc;
	D230.cupSel_transitionState = 0;

	// disable 0x400 (dont exec funcptr)
	// enable 0x20 (allow exec funcptr, and block input
	D230.menuCupSelect.state &= ~(EXECUTE_FUNCPTR);
	D230.menuCupSelect.state |= DISABLE_INPUT_ALLOW_FUNCPTRS;
}
