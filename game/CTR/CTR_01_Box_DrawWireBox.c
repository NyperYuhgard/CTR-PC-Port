#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80021594-0x8002177c.
void CTR_Box_DrawWireBox(RECT *r, const Color *color, void *ot, struct PrimMem *primMem)
{
	LineF3 *p = primMem->curr;
	if (p > (LineF3 *)primMem->endMin100)
	{
		return;
	}
	primMem->curr = p + 1;

	const PrimCode primCode = {.line = {.renderCode = RenderCode_Line, .polyline = 1}};
	p->tag.size = (sizeof(*p) - sizeof(p->tag)) / sizeof(u32);
	p->colorCode = *color;
	p->colorCode.code = primCode;

	s16 topX = r->x;
	s16 topY = r->y;
	s16 bottomX = r->x + r->w;
	s16 bottomY = r->y + r->h;
	p->v[0].pos.x = topX;
	p->v[0].pos.y = topY;
	p->v[1].pos.x = bottomX;
	p->v[1].pos.y = topY;
	p->v[2].pos.x = bottomX;
	p->v[2].pos.y = bottomY;
	p->end = 0x55555555;

	AddPrimitive(p, ot);
	p = primMem->curr;
	if (p > (LineF3 *)primMem->endMin100)
	{
		return;
	}
	primMem->curr = p + 1;

	p->tag.size = (sizeof(*p) - sizeof(p->tag)) / sizeof(u32);
	p->colorCode = *color;
	p->colorCode.code = primCode;
	p->v[0].pos.x = topX;
	p->v[0].pos.y = topY;
	p->v[1].pos.x = topX;
	p->v[1].pos.y = bottomY;
	p->v[2].pos.x = bottomX;
	p->v[2].pos.y = bottomY;
	p->end = 0x55555555;

	AddPrimitive(p, ot);
}
