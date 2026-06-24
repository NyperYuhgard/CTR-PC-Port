#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004b31c-0x8004b370.
void Timer_Init()
{
	EnterCriticalSection();
	StopRCnt(DescRC + 1);
	SetRCnt(DescRC + 1, 0xffff, 0x2000);
	StartRCnt(DescRC + 1);
	ExitCriticalSection();
}
