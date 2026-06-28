#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8004b370-0x8004b3a4.
void Timer_Destroy()
{
	EnterCriticalSection();
	StopRCnt(DescRC + 1);
	ExitCriticalSection();
}
