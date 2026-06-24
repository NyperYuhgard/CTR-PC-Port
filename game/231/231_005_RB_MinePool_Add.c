#include <common.h>

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 231 0x800ac13c-0x800ac1b0.
void RB_MinePool_Add(struct MineWeapon *mw)
{
	struct WeaponSlot231 *ws;

	// if no more items on free list
	if ((&D231.minePoolFree)->count == 0)
	{
		// remove oldest mine
		RB_MinePool_Remove(((struct WeaponSlot231 *)(&D231.minePoolTaken)->last)->mineWeapon);
	}

	// LIST_RemoveBack free list
	ws = (struct WeaponSlot231 *)LIST_RemoveBack(&D231.minePoolFree);

	// link together
	ws->mineWeapon = mw;
	mw->weaponSlot231 = ws;

	// LIST_AddFront to taken list
	LIST_AddFront(&D231.minePoolTaken, (struct Item *)ws);
}
