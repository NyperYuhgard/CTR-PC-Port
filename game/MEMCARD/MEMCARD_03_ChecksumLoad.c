#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003d618-0x8003d6e8.
int MEMCARD_ChecksumLoad(u8 *saveBytes, int len)
{
	int byteIndex = sdata->crc16_checkpoint_byteIndex;
	int byteIndexEnd;
	int boolFinishThisFrame;
	int crc = sdata->crc16_checkpoint_status;

	if ((sdata->memcardStatusFlags & MEMCARD_STATUS_SYNC_CHECKSUM) == 0)
	{
		byteIndexEnd = byteIndex + 0x200;
		boolFinishThisFrame = 0;

		if (byteIndexEnd < len - 2)
			goto RunChecksum;
	}

	boolFinishThisFrame = 1;
	byteIndexEnd = len - 2;

RunChecksum:
	for (; byteIndex < byteIndexEnd; byteIndex++)
	{
		crc = MEMCARD_CRC16(crc, saveBytes[byteIndex]);
	}

	sdata->crc16_checkpoint_byteIndex = byteIndex;
	sdata->crc16_checkpoint_status = crc;

	if (boolFinishThisFrame == 0)
		return MC_RETURN_PENDING;

	crc = MEMCARD_CRC16(crc, saveBytes[byteIndex]);
	crc = MEMCARD_CRC16(crc, saveBytes[byteIndex + 1]);

	// Will return one of these:
	// 0: MC_RETURN_IOE
	// 1: MC_RETURN_TIMEOUT
	return (u32)(crc != 0);
}
