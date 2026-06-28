#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80058ec0-0x80058f54.
struct Driver *VehBirth_Player(int index)
{
	struct Thread *t = PROC_BirthWithObject(0x62c0100, 0, sdata->s_player, 0);

	struct Driver *d = t->object;
	memset(d, 0, 0x62c);

	VehBirth_NonGhost(t, index);

	d->funcPtrs[0] = VehPhysProc_Driving_Init;

	d->BattleHUD.teamID = sdata->gGT->battleSetup.teamOfEachPlayer[index];

	return d;
}
