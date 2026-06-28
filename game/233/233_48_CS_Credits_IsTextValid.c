#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b92a0-0x800b92cc
int CS_Credits_IsTextValid(void)
{
	struct CreditsObj *creditsObj = &creditsBSS.creditsObj;

	if (creditsObj->epilogue_topString != 0)
		return 0;

	creditsObj->countdown = 360;
	return 1;
}
