#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004ca04-0x8004caa8.
void UI_ThTick_big1(struct Thread *bucket)

{
	s16 uVar1;
	u32 flags;
	struct UiElement3D *obj;
	struct Instance *inst;

	// Get object from thread
	obj = bucket->object;

	// Get instance from thread
	inst = bucket->inst;

	uVar1 = obj->rot[3];
	*(int *)&inst->matrix.m[0][0] = uVar1;
	*(int *)&inst->matrix.m[0][2] = 0;
	*(int *)&inst->matrix.m[1][1] = uVar1;
	*(int *)&inst->matrix.m[2][0] = 0;
	inst->matrix.m[2][2] = uVar1;

	MatrixRotate(&inst->matrix, &obj->m, &inst->matrix);

	// if hud is enabled, and this is not demo mode
	if ((*(int *)&sdata->gGT->bool_DrawOTag_InProgress & 0xff0100) == 0x100)
	{
		// make visible
		flags = inst->flags & 0xffffff7f;
	}

	else
	{
		// make invisible
		flags = inst->flags | 0x80;
	}

	inst->flags = flags;

	return;
}
