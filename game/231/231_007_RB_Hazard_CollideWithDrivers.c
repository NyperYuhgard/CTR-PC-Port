#include <common.h>

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 231 0x800ac220-0x800ac350.
struct Instance *RB_Hazard_CollideWithDrivers(struct Instance *weaponInst, char boolCanSkipParent, int hitRadius, struct Instance *mineDriverInst)
{
	int j;
	struct Driver *driver;
	struct Instance *driverInst;

	u32 dist[3];
	int modelID;

	u32 distCheck;
	s32 delta;

	for (int i = 0; i < 8; i++)
	{
		driver = sdata->gGT->drivers[i];

		if (driver == 0)
			continue;
		if (driver->kartState == KS_MASK_GRABBED)
			continue;
		driverInst = driver->instSelf;

		for (j = 0; j < 3; j++)
		{
			delta = driverInst->matrix.t[j] - weaponInst->matrix.t[j];
			dist[j] = (u32)delta * (u32)delta;
		}

		modelID = weaponInst->model->id;

		// 2D collision (barrel, warpball)
		distCheck = dist[0] + dist[2];

		// to be more optimal, just do weaponInst->thread->funcThTick == GenericMine_ThTick
		if (((u32)modelID - STATIC_BEAKER_RED < 2) || // red or green potion
		    (modelID == PU_EXPLOSIVE_CRATE) ||        // Nitro
		    (modelID == STATIC_CRATE_TNT)             // TNT
		)
		{
			// upgrade to 2D collision to 3D sphere
			distCheck += dist[1];
		}

		// 2D collision, or 3D sphere
		if (distCheck < (u32)hitRadius)
		{
			if ((boolCanSkipParent != 0) && (driverInst == mineDriverInst))
				continue;

			// wasted check for 3D sphere,
			// also upgrades 2D collision to 3D cylinder
			if ((s32)dist[1] < (s32)((u32)hitRadius << 2))
			{
				return driverInst;
			}
		}
	}

	return 0;
}
