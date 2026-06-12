#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003b334-0x8003b43c.
void MainInit_OTMem(struct GameTracker *gGT)
{
	int size;
	u32 gameMode = gGT->gameMode1;

	if ((gameMode & MAIN_MENU) != 0)
	{
		size = 0x1800;
		goto EndFunc;
	}

	if ((gameMode & ADVENTURE_ARENA) != 0)
	{
		size = 0x2c00;
		goto EndFunc;
	}

	if ((gameMode & BATTLE_MODE) != 0)
	{
		size = 0x8000;
		goto EndFunc;
	}

	// 1P/2P mode
	if (gGT->numPlyrCurrGame < 3)
	{
		size = 0x2000;
		goto EndFunc;
	}

	// 3P/4P mode
	size = 0x3000;

EndFunc:

	MainDB_OTMem(&gGT->db[0].otMem, size);
	MainDB_OTMem(&gGT->db[1].otMem, size);

	// 0x1000 per player, plus 0x18 for linking
	size = ((gGT->numPlyrCurrGame) << 0xC) | 0x18;
	gGT->otSwapchainDB[0] = MEMPACK_AllocMem(size); // "ot1"
	gGT->otSwapchainDB[1] = MEMPACK_AllocMem(size); // "ot2"
}
