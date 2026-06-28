#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80021e1c-0x80021ea8.
void CTR_unknownMaybeThunk2(void *dst, void *src)
{
	u8 *out = (u8 *)dst;
	s8 *rle = (s8 *)src;

	for (;;)
	{
		int count = *rle;

		if (count == 0)
			return;

		if (count < 0)
		{
			int repeat = 1 - count;
			u8 value = (u8)rle[1];
			rle += 2;

			while (repeat-- != 0)
				*out++ |= value;
		}

		else
		{
			rle++;

			while (count-- != 0)
				*out++ |= (u8)*rle++;
		}
	}
}
