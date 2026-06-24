#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004dffc-0x8004e0e0.
void UI_Map_DrawTracking(int ptrMap, struct Thread *bucket)

{
	int uVar1;
	struct Instance *inst;
	struct TrackerWeapon *tw;
	struct Driver *d;

	for (/* bucket */; bucket != 0; bucket = bucket->siblingThread)
	{
		// thread -> instance
		inst = bucket->inst;

		// instance -> model -> modelID != warpball
		if (inst->model->id != DYNAMIC_WARPBALL)
			continue;

		// == only draw warpball ==

		// draw warpball
		UI_Map_DrawRawIcon(ptrMap, (int *)&inst->matrix.t[0], 0x20, 0, 0, 0x1000);

		// driver target
		tw = (struct TrackerWeapon *)inst->thread->object;
		d = tw->driverTarget;

		// check if target exists
		if (d == 0)
			continue;

		// == only draw target if target exists ==

		// flicker
		uVar1 = 4;
		if ((sdata->gGT->timer & 1) != 0)
			uVar1 = 3;

		UI_Map_DrawRawIcon(ptrMap, (int *)&d->instSelf->matrix.t[0], 0x21, uVar1, 0, 0x1000);
	}
	return;
}
