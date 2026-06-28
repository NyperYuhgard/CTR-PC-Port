#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003dbf8-0x8003dc30.
void MEMCARD_CloseFile(void)
{
	int fd = sdata->memcard_fd;

	if (fd != -1)
	{
		close(fd);
		sdata->memcard_fd = -1;
	}

	sdata->memcard_stage = MC_STAGE_IDLE;
}
