#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 PSX path 0x80035d30-0x80035d70.
void MainFrame_InitVideoSTR(u32 boolPlayVideoStr, RECT *r, s16 posX, s16 posY)
{
#ifdef CTR_NATIVE
	if (r == NULL)
	{
		// NOTE(aalhendi): Native overlay 230 uses NULL to disable STR copy;
		// retail blindly copies the RECT fields in the PSX path below.
		sdata->videoSTR_src_vramRect.x = 0;
		sdata->videoSTR_src_vramRect.y = 0;
		sdata->videoSTR_src_vramRect.w = 0;
		sdata->videoSTR_src_vramRect.h = 0;
	}
	else
#endif
	{
		sdata->videoSTR_src_vramRect.x = r->x;
		sdata->videoSTR_src_vramRect.y = r->y;
		sdata->videoSTR_src_vramRect.w = r->w;
		sdata->videoSTR_src_vramRect.h = r->h;
	}

	sdata->boolPlayVideoSTR = boolPlayVideoStr;
	sdata->videoSTR_dst_vramX = posX;
	sdata->videoSTR_dst_vramY = posY;
}
