#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b56c4-0x800b57b4.
void RB_Banner_Animate_Play(struct ModelHeader *mh, s16 numVertices)
{
	u32 *colors = mh->ptrColors;
	u32 firstColor = colors[0];
	u8 *vertex;

	for (int i = 0; i < 0x3f; i++)
		colors[i] = colors[i + 1];
	colors[0x3f] = firstColor;

	if (numVertices <= 0)
		return;

	vertex = (u8 *)mh->ptrFrameData + mh->ptrFrameData->vertexOffset;
	for (int i = 0; i < numVertices; i++, vertex += 3)
	{
		u8 x = vertex[0];
		u8 color = ((u8 *)mh->ptrColors)[(((x >> 2) + 10) & 0x3f) * 4];
		int wave = (int)color - 0x80;

		if (x < 0x40)
			wave = (wave * ((int)x << 2)) >> 8;
		else if (x > 0xc0)
			wave = (wave * ((0x100 - (int)x) << 2)) >> 8;

		vertex[1] = (u8)(wave + 0x80);
	}
}
