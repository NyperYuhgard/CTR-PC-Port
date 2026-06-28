#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004e8d8-0x8004eaa8.
void UI_DrawDriverIcon(struct Icon *icon, s16 posX, s16 posY, struct PrimMem *primMem, u_long *ot, char transparency, s16 scale, u32 color)
{
	PolyFT4 *p = primMem->curr;
	const PrimCode primCode = {.poly = {.renderCode = RenderCode_Polygon, .quad = 1, .textured = 1}};
	p->colorCode.self = color;
	p->colorCode.code = primCode;

	int width = icon->texLayout.u1 - icon->texLayout.u0;
	int height = icon->texLayout.v2 - icon->texLayout.v0;
	int topX = posX;
	int bottomX = topX + FP_Mult(width, scale);
#if BUILD != EurRetail
	int topY = (posY < 166) ? posY : 165;
	int bottomY = ((posY + FP_Mult(height, scale)) < 166) ? (posY + FP_Mult(height, scale)) : 165;
#else
	int topY = (posY < 176) ? posY : 175;
	int bottomY = ((posY + FP_Mult(height, scale)) < 176) ? (posY + FP_Mult(height, scale)) : 175;
#endif

	p->tag.size = (sizeof(*p) - sizeof(p->tag)) / sizeof(u32);
	p->colorCode.code.code = 0x2c;

	p->v[0].pos.x = topX;
	p->v[0].pos.y = topY;
	p->v[1].pos.x = bottomX;
	p->v[1].pos.y = topY;
	p->v[2].pos.x = topX;
	p->v[2].pos.y = bottomY;
	p->v[3].pos.x = bottomX;
	p->v[3].pos.y = bottomY;

	p->polyClut.self = icon->texLayout.clut;
	p->polyTpage.self = icon->texLayout.tpage;
	p->v[2].clut.self = (icon->texLayout.v3 << 8) | icon->texLayout.u3;

	if (transparency)
	{
		p->polyTpage.semiTransparency = transparency - 1;
		p->colorCode.code.poly.semiTransparency = 1;
	}

	u32 bottomV = (icon->texLayout.v0 + bottomY) - posY;
	p->v[0].texCoords.u = icon->texLayout.u0;
	p->v[0].texCoords.v = icon->texLayout.v0;
	p->v[1].texCoords.u = icon->texLayout.u1;
	p->v[1].texCoords.v = icon->texLayout.v1;
	p->v[2].texCoords.u = icon->texLayout.u2;
	p->v[2].texCoords.v = bottomV;
	p->v[3].texCoords.u = icon->texLayout.u3;
	p->v[3].texCoords.v = bottomV;

	AddPrimitive(p, ot);
	primMem->curr = p + 1;
}
