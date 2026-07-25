#include <common.h>
#include <platform/native_input.h>

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 230 0x800ad560-0x800ad5e8.
void MM_MenuProc_1p2p(struct RectMenu *menu)

{
	s16 row;

	struct GameTracker *gGT;
	gGT = sdata->gGT;

	row = menu->rowSelected;

	// if uninitialized
	if (row == -1)
	{
		menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);

		gGT->numPlyrNextGame = 1;

		D230.characterSelect_transitionState = 0;
	}

	else
	{
		// if on row 0 or 1
		if ((row >= 0) && (row < 2))
		{
			// row 0 is 1P, row 1 is 2P
			gGT->numPlyrNextGame = menu->rowSelected + 1;

#ifdef CTR_NATIVE
			Platform_InputSetMaxPlayers(gGT->numPlyrNextGame);
#endif

			// Adventure mode: skip difficulty, go directly to garage or character select
			if ((gGT->gameMode1 & ADVENTURE_MODE) != 0)
			{
				gGT->gameMode2 &= ~COOPERATIVE_ADVENTURE;

				if (menu->rowSelected == 1)
				{
					gGT->gameMode2 |= COOPERATIVE_ADVENTURE;
				}

				D230.desiredMenuIndex = 0;
				D230.MM_State = 2;
				menu->state |= 4;
				return;
			}

			// go to difficulty box
			menu->ptrNextBox_InHierarchy = &D230.menuDifficulty;

			menu->state |= 0x14;
			return;
		}
	}
	return;
}
