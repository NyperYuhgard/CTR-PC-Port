#include <common.h>

// Draw various objects, like relic,
// key trophy, token, crystal, etc
// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004c850-0x8004c914.
void UI_ThTick_Reward(struct Thread *bucket)

{
	u32 flags;
	MATRIX *mat;
	struct GameTracker *gGT;
	struct UiElement3D *obj;
	struct Instance *inst;

	gGT = sdata->gGT;

	// Get instance
	inst = bucket->inst;

	// Get object
	obj = bucket->object;

	// Spin on the Y axis
	obj->rot[1] += 0x40;

	Vector_SpecLightSpin2D(inst, (s16 *)obj->rot, (s16 *)obj->lightDir);

	// pointer to matrix
	mat = &inst->matrix;

	ConvertRotToMatrix(mat, (s16 *)obj->rot);

	MatrixRotate(mat, &obj->m, mat);

	if (
	    // if hud is enabled, and this is not demo mode
	    ((*(int *)&gGT->bool_DrawOTag_InProgress & 0xff0100) == 0x100) &&

	    // if any fade-in-from-black transition is over
	    (0xfff < gGT->pushBuffer_UI.fadeFromBlack_currentValue))
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
