#include <common.h>

static char s_startbanner[] = "startbanner";

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b57f8-0x800b5968.
void RB_Banner_LInB(struct Instance *inst)
{
	struct GameTracker *gGT = sdata->gGT;
	struct Thread *t;
	struct StartBanner *banner;
	struct Model *model;

	if (inst->thread != NULL)
		return;

	t = PROC_BirthWithObject(SIZE_RELATIVE_POOL_BUCKET(sizeof(struct StartBanner), NONE, SMALL, STATIC), RB_Banner_ThTick, s_startbanner, NULL);
	inst->thread = t;
	if (t == NULL)
		return;

	if (gGT->numPlyrCurrGame >= 4)
		t->funcThTick = NULL;

	banner = t->object;
	t->inst = inst;
	banner->unused = 0;
	banner->numVertices = 0;

	model = gGT->modelPtr[STATIC_STARTBANNERWAVE];
	if (model == NULL)
		return;

	inst->model = model;
	banner->numVertices = RB_Banner_Animate_Init(model->headers);
	if (banner->numVertices == 0)
		return;

	for (int i = 0; i < 0x40; i++)
	{
		u8 *color = (u8 *)&model->headers->ptrColors[i];
		int value = (MATH_Sin((u32)i << 7) >> 6) + 0x80;

		if (gGT->numPlyrCurrGame >= 4)
			value = 0x80;

		color[0] = (u8)value;
		color[1] = (u8)value;
		color[2] = (u8)value;
	}
}
