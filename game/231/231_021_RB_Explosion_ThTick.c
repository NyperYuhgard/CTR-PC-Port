#include <common.h>

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 231 0x800ad92c-0x800ad9ac.
void RB_Explosion_ThTick(struct Thread *t)
{
	struct Instance *inst = t->inst;

	int frame = inst->animFrame;
	int total = INSTANCE_GetNumAnimFrames(inst, 0);

	if ((frame + 1) < total)
	{
#ifdef CTR_NATIVE
		{ static int s_60fpsExplosionToggle = 0; if (!IS_NATIVE_60FPS || (s_60fpsExplosionToggle ^= 1)) inst->animFrame++; }
#else
		inst->animFrame++;
#endif
	}
	else
	{
		// dead thread
		t->flags |= 0x800;
	}

	ThTick_FastRET(t);
}
