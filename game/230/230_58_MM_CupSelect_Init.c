#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b0eb8-0x800b0eec.
void MM_CupSelect_Init(void)
{
	// reset transition data
	D230.cupSel_transitionFrames = 0xc;
	D230.cupSel_transitionState = 0;
	D230.cupSel_scrollOffset = 0;

	// If Oxide Cup is unlocked, modify rows array to include 5th row
	if ((sdata->gameProgress.unlocks[1] & (1 << 5)) != 0)
	{
		// Replace terminator at index 4 with Oxide Cup row
		// Pressing Down from cup 3 goes to cup 4; Up from cup 4 goes back to cup 2
		D230.rowsCupSelect[3].rowOnPressDown = 4;
		D230.rowsCupSelect[4].stringIndex = 0;
		D230.rowsCupSelect[4].rowOnPressUp = 2;
		D230.rowsCupSelect[4].rowOnPressDown = 4;
		D230.rowsCupSelect[4].rowOnPressLeft = 4;
		D230.rowsCupSelect[4].rowOnPressRight = 4;
		// Add new terminator at index 5
		D230.rowsCupSelect[5].stringIndex = -1;
	}
	else
	{
		// Ensure terminator is at index 4
		D230.rowsCupSelect[4].stringIndex = -1;
	}

	// disable 0x400 (dont exec funcptr)
	// enable 0x20 (allow exec funcptr, and block input
	D230.menuCupSelect.state &= ~(EXECUTE_FUNCPTR);
	D230.menuCupSelect.state |= DISABLE_INPUT_ALLOW_FUNCPTRS;
}
