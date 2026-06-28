#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b57b4-0x800b57f8.
void RB_Banner_ThTick(struct Thread *t)
{
	struct StartBanner *banner = t->object;

	if (banner->numVertices != 0)
		RB_Banner_Animate_Play(t->inst->model->headers, banner->numVertices);
}
