#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b5984-0x800b5f50.

int RB_Armadillo_ThCollide(struct Thread *armadilloThread, struct Thread *driverTh, void *funcThCollide, struct ScratchpadStruct *sps)
{
	(void)armadilloThread;
	(void)driverTh;
	(void)funcThCollide;

	return (s16)sps->Input1.modelID == DYNAMIC_PLAYER;
}

void RB_Armadillo_ThTick_TurnAround(struct Thread *t)
{
	struct Instance *armInst;
	struct Armadillo *armObj;

	armInst = t->inst;
	armObj = (struct Armadillo *)t->object;

	if (armObj->rotCurr[1] == armObj->rotDesired[1])
	{
		if ((armInst->animFrame + 1) < INSTANCE_GetNumAnimFrames(armInst, 0))
		{
			if (!IS_NATIVE_60FPS || (armObj->s_60fpsToggle ^= 1))
				armInst->animFrame = armInst->animFrame + 1;
		}
		else
		{
			armObj->velX = -armObj->velX;
			armObj->velZ = -armObj->velZ;

			armObj->direction = (armObj->direction == 0) ? 1 : 0;

			PlaySound3D(0x70, armInst);

			armInst->animIndex = 1;
			armInst->animFrame = 0;

			ThTick_SetAndExec(t, RB_Armadillo_ThTick_Rolling);
		}
	}
	else
	{
		armObj->rotCurr[1] = RB_Hazard_InterpolateValue(armObj->rotCurr[1], armObj->rotDesired[1], 0x100);

		ConvertRotToMatrix(&armInst->matrix, &armObj->rotCurr[0]);

		if (!IS_NATIVE_60FPS || (armObj->s_60fpsToggle ^= 1))
			armInst->animFrame = armInst->animFrame + 1;
	}

	Seal_CheckColl(armInst, t, 1, 0x2400, 0x71);
}

void RB_Armadillo_ThTick_Rolling(struct Thread *t)
{
	struct Instance *armInst;
	struct Armadillo *armObj;
	SVECTOR rot;

	armInst = t->inst;
	armObj = (struct Armadillo *)t->object;

	if (armObj->timeAtEdge != 0)
	{
		if (!IS_NATIVE_60FPS || (armObj->s_60fpsToggle ^= 1))
			armObj->timeAtEdge--;
		return;
	}

	int doTick = !IS_NATIVE_60FPS || (armObj->s_60fpsToggle ^= 1);

	if (armObj->timeRolling < 0x500)
	{
		armObj->timeRolling += FPS_HALF(0x20);

		if (armObj->direction == 0)
			armObj->distFromSpawn++;
		else
			armObj->distFromSpawn--;

		if (doTick)
		{
			armInst->matrix.t[0] += armObj->velX;
			armInst->matrix.t[2] += armObj->velZ;
		}

		if ((armInst->animFrame + 1) < INSTANCE_GetNumAnimFrames(armInst, 1))
		{
			if (doTick)
				armInst->animFrame = armInst->animFrame + 1;
		}
		else
		{
			armInst->animFrame = 0;
		}

		Seal_CheckColl(armInst, t, 1, 0x2400, 0x71);
		return;
	}

	CTR_MatrixToRot(&rot, &armInst->matrix, 0x11);

	armObj->rotCurr[0] = rot.vy;
	armObj->rotCurr[1] = rot.vx;
	armObj->rotCurr[2] = rot.vz;
	armObj->timeRolling = 0;

	armInst->animIndex = 0;
	armInst->animFrame = 0;

	armObj->rotDesired[1] = (armObj->rotCurr[1] + 0x800) & 0xfff;

	ThTick_SetAndExec(t, RB_Armadillo_ThTick_TurnAround);
}

void RB_Armadillo_LInB(struct Instance *inst)
{
	struct Armadillo *armObj;
	SVECTOR rot;
	s16 *metaArray;
	void **pointers;
	struct Thread *t;

	if (inst->thread != 0)
		return;

	t = PROC_BirthWithObject(
	    SIZE_RELATIVE_POOL_BUCKET(sizeof(struct Armadillo), NONE, SMALL, STATIC),
	    RB_Armadillo_ThTick_Rolling,
	    "armadillo",
	    0
	);

	if (t == 0)
		return;
	inst->thread = t;
	t->inst = inst;
	t->funcThCollide = (void (*)(struct Thread *))RB_Armadillo_ThCollide;

	inst->animIndex = 1;

	armObj = ((struct Armadillo *)t->object);
	armObj->timeRolling = 0;
	armObj->s_60fpsToggle = 0;
	armObj->timeAtEdge = 0;

	CTR_MatrixToRot(&rot, &inst->matrix, 0x11);
	armObj->rotCurr[0] = rot.vy;
	armObj->rotCurr[1] = rot.vx;
	armObj->rotCurr[2] = rot.vz;

	armObj->rotDesired[1] = (armObj->rotCurr[1] + 0x800) & 0xfff;

	armObj->distFromSpawn = 0;
	armObj->spawnPosX = inst->matrix.t[0];
	armObj->spawnPosZ = inst->matrix.t[2];
	armObj->direction = 0;

	armObj->velX = inst->matrix.m[0][2] >> 7;
	armObj->velZ = inst->matrix.m[2][2] >> 7;

	if (sdata->gGT->level1->ptrSpawnType1->count <= 0)
		return;

	// puts armadillos on separate cycles
	pointers = ST1_GETPOINTERS(sdata->gGT->level1->ptrSpawnType1);
	metaArray = (s16 *)pointers[ST1_SPAWN];
	armObj->timeAtEdge = metaArray[inst->name[strlen(inst->name) - 1] - '0'];
}
