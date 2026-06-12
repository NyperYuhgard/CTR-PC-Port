#include <common.h>

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 231 0x800b38e4-0x800b3978.
void RB_Blade_ThTick(struct Thread *t)
{
	struct Blade *bladeObj;
	struct Instance *bladeInst;
	s16 rot[3];

	bladeObj = (struct Blade *)t->object;
	bladeInst = t->inst;

	rot[0] = bladeInst->instDef->rot[0];
	rot[1] = bladeInst->instDef->rot[1] + 0x400;
	rot[2] = bladeObj->angle;

	bladeObj->angle += 0x100;

	// converted to TEST in rebuildPS1
	ConvertRotToMatrix(&bladeInst->matrix, &rot[0]);

	bladeInst->scale[0] = 0x1000;
	bladeInst->scale[1] = 0x1000;
	bladeInst->scale[2] = 0x1000;

	ThTick_FastRET(t);
}
