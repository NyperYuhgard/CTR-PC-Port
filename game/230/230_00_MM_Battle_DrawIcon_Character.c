#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800abaa8-0x800abaf0.
void MM_Battle_DrawIcon_Character(struct Icon *icon, int posX, int posY, struct PrimMem *primMem, u_long *ot, char transparency, s16 scale)
{
	if (icon == 0)
		return;
	DecalHUD_DrawPolyFT4(icon, posX, posY, primMem, ot, transparency, scale);
}
