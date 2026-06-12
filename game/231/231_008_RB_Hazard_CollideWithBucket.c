#include <common.h>

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 231 0x800ac350-0x800ac3f8.
struct Instance *RB_Hazard_CollideWithBucket(struct Instance *weaponInst, struct Thread *weaponTh, struct Thread *bucket, char boolCanSkipParent, int hitRadius,
                                             struct Instance *mineDriverInst)
{
	int i;
	struct Instance *threadInst;

	s32 distComponent;
	u32 distCheck;

	(void)weaponTh;

	for (; bucket != 0; bucket = bucket->siblingThread)
	{
		distCheck = 0;
		threadInst = bucket->inst;

		for (i = 0; i < 3; i++)
		{
			distComponent = threadInst->matrix.t[i] - weaponInst->matrix.t[i];
			distCheck += (u32)distComponent * (u32)distComponent;
		}

		if (distCheck < (u32)hitRadius)
		{
			if ((boolCanSkipParent != 0) && (threadInst == mineDriverInst))
				continue;

			return threadInst;
		}
	}

	return 0;
}
