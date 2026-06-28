#include <common.h>

static struct PushBuffer *VehPickupItem_GetDriverPushBuffer(struct GameTracker *gGT, u8 driverID)
{
	return (struct PushBuffer *)((u8 *)&gGT->pushBuffer[0] + (driverID * sizeof(struct PushBuffer)));
}

static void VehPickupItem_MissileLoadPlayerView(struct GameTracker *gGT, struct Driver *driver)
{
	struct PushBuffer *pb = VehPickupItem_GetDriverPushBuffer(gGT, driver->driverID);

	SetRotMatrix(&pb->matrix_ViewProj);
	SetTransMatrix(&pb->matrix_ViewProj);
}

static void VehPickupItem_MissileLoadAiView(struct Driver *driver)
{
	SVECTOR rot = {driver->rotCurr.x, driver->rotCurr.y, driver->rotCurr.z, 0};
	MATRIX matrix;
	MATRIX unusedInverse;

	RotMatrix(&rot, &matrix);
	matrix.t[0] = driver->posCurr.x >> 8;
	matrix.t[1] = driver->posCurr.y >> 8;
	matrix.t[2] = driver->posCurr.z >> 8;

	MATH_HitboxMatrix(&unusedInverse, &matrix);

	SetRotMatrix(&matrix);
	SetTransMatrix(&matrix);
}

static int VehPickupItem_MissileCandidateVisible(struct PushBuffer *pb, struct Driver *candidate)
{
	struct Instance *inst = candidate->instSelf;
	u32 sxy;
	s32 gteFlag;
	s16 screenX;
	s16 screenY;

	MTC2(((u32)(u16)inst->matrix.t[0]) | ((u32)(u16)inst->matrix.t[1] << 16), 0);
	MTC2((s32)(s16)inst->matrix.t[2], 1);
	gte_rtps();

	sxy = MFC2(14);
	gteFlag = CFC2(31);
	if ((gteFlag & 0x40000) != 0)
		return 0;

	screenX = (s16)sxy;
	if (screenX < 0x1f)
		return 0;
	if (screenX >= pb->rect.w - 0x1e)
		return 0;

	screenY = (s16)(sxy >> 16);
	if (screenY < 0x15)
		return 0;
	if (screenY >= pb->rect.h - 0x14)
		return 0;

	return 1;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80064f94-0x800652c8.
struct Driver *VehPickupItem_MissileGetTargetDriver(struct Driver *driver)
{
	struct GameTracker *gGT = sdata->gGT;
	struct Driver *target = NULL;
	s32 closestDistance = 0x7fffffff;
	struct PushBuffer *pb = VehPickupItem_GetDriverPushBuffer(gGT, driver->driverID);

	if (driver->instSelf->thread->modelIndex == DYNAMIC_PLAYER)
	{
		VehPickupItem_MissileLoadPlayerView(gGT, driver);
	}
	else
	{
		VehPickupItem_MissileLoadAiView(driver);
	}

	for (int i = 0; i < 8; i++)
	{
		struct Driver *candidate = gGT->drivers[i];

		if (candidate == NULL)
			continue;
		if (candidate == driver)
			continue;
		if (candidate->kartState == KS_MASK_GRABBED)
			continue;

		if (((gGT->gameMode1 & BATTLE_MODE) != 0) && (candidate->BattleHUD.teamID == driver->BattleHUD.teamID))
			continue;

		if (candidate->invisibleTimer != 0)
			continue;

		if (!VehPickupItem_MissileCandidateVisible(pb, candidate))
			continue;

		s32 dx = (candidate->posCurr.x - driver->posCurr.x) >> 8;
		s32 dz = (candidate->posCurr.z - driver->posCurr.z) >> 8;
		s32 distance = (dx * dx) + (dz * dz);
		if (distance < closestDistance)
		{
			closestDistance = distance;
			target = candidate;
		}
	}

	return target;
}
