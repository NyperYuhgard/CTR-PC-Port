#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80021ea8-0x80021edc.
void CTR_unknownMaybeThunk3(void *dst, void *src, int byteCount)
{
	u32 *out = (u32 *)dst;
	u32 *in = (u32 *)src;
	u32 wordCount = (u32)(byteCount >> 2);

	while (wordCount != 0)
	{
		*out++ |= *in++;
		wordCount--;
	}
}
