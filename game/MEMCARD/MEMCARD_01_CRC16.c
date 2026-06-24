#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003d540-0x8003d584.
u32 MEMCARD_CRC16(u32 crc, int nextByte)
{
	int i;
	int bitCheck;

	for (i = 7; i >= 0; i--)
	{
		bitCheck = crc << 1;
		crc = bitCheck | nextByte >> i & 1;

		if ((bitCheck & 0x10000) != 0)
		{
			crc = crc ^ 0x11021;
		}
	}

	return crc;
}
