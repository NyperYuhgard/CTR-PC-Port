#include <common.h>

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 231 0x800ac0e4-0x800ac13c.
void RB_MinePool_Remove(struct MineWeapon *mw)
{
	struct WeaponSlot231 *ws = mw->weaponSlot231;

	if (ws == NULL)
		return;

	// remove from taken, add to free
	LIST_RemoveMember(&D231.minePoolTaken, (struct Item *)ws);
	LIST_AddFront(&D231.minePoolFree, (struct Item *)ws);

	mw->boolDestroyed = 1;
	mw->weaponSlot231 = NULL;
}
