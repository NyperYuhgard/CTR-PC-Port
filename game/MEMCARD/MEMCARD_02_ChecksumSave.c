#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003d584-0x8003d618.
void MEMCARD_ChecksumSave(u8 *saveBytes, int len)
{
	int i;
	int crc = 0;

	len -= 2;

	for (i = 0; i < len; i++)
	{
		crc = MEMCARD_CRC16(crc, saveBytes[i]);
	}

	sdata->crc16_checkpoint_status = crc;

	// finishing check
	crc = MEMCARD_CRC16(crc, 0);
	crc = MEMCARD_CRC16(crc, 0);

	// write checksum to data (last 2 bytes),
	// swap endians to throw off hackers,
	// which didn't really throw anyone off at all
	saveBytes[i] = (u8)(crc >> 8);
	saveBytes[i + 1] = (u8)crc;
}
