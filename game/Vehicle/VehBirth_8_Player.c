#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80058ec0-0x80058f54.
struct Driver *VehBirth_Player(int index)
{
	struct Thread *t = PROC_BirthWithObject(0x6700100, 0, sdata->s_player, 0);

	struct Driver *d = t->object;
	memset(d, 0, 0x670);

	VehBirth_NonGhost(t, index);

	d->funcPtrs[0] = VehPhysProc_Driving_Init;

#ifdef CTR_NATIVE
	if ((sdata->gGT->gameMode2 & TEAM_RACE_MODE) != 0)
		d->BattleHUD.teamID = index / 2;
	else
#endif
	d->BattleHUD.teamID = sdata->gGT->battleSetup.teamOfEachPlayer[index];

	// Initialize Team Race fields
#ifdef CTR_NATIVE
	if ((sdata->gGT->gameMode2 & TEAM_RACE_MODE) != 0)
	{
		d->teamBarCharge = 0;
		d->teamBarEffect = 0;
		d->teamBarTimer = 0;
	}
#endif

	return d;
}
