#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b9398-0x800b93f4
int CS_Credits_NewCreditGhosts(void)
{
	struct Model *model = creditsBSS.dancerInst_invisible->model;
	int i;

	for (i = 0; i < 5; i++)
	{
		if (creditsBSS.creditsObj.creditGhostModel[i] != model)
			return 0;
	}

	return 1;
}
