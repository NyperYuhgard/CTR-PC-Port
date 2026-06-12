#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b344c-0x800b351c.
void AH_HintMenu_FiveArrows(int param_1, s16 rotation)
{
	int *ptrColor;
	char i;

	ptrColor = &D232.fiveArrow_col1[0];
	if ((sdata->frameCounter & 2) != 0)
		ptrColor = &D232.fiveArrow_col2[0];

	for (i = 0; i < 5; i++)
	{
		AH_Map_HubArrow(
		    // posX
		    (i * 0x32 + 0x95),

		    // posY
		    (param_1 + 4),

		    &D232.fiveArrow_pos[0],

		    (char *)ptrColor, 0x800, (int)rotation);
	}
}
