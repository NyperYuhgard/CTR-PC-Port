#include <common.h>

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 230 0x800ad8f0-0x800ad980.
void MM_MenuProc_NewLoad(struct RectMenu *menu)
{
	s16 row;

	// row number
	row = menu->rowSelected;

	if (row == -1)
	{
		menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		return;
	}

	if ((row < 0) || (row > 1))
		return;

#ifdef CTR_NATIVE
	// If "New" was chosen, show 1P/2P menu (reused from Arcade/VS)
	if (row == 0)
	{
		menu->state |= ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY;
		menu->ptrNextBox_InHierarchy = &D230.menuPlayers1P2P;
		D230.characterSelect_transitionState = 1;
		return;
	}
#endif

	// if Load was chosen
	D230.desiredMenuIndex = row;

	// MM_Title transitioning out
	D230.MM_State = 2;

	menu->state |= 4;
	return;
}
