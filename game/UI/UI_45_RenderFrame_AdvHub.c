#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80054298-0x8005435c.
void UI_RenderFrame_AdvHub(void)
{
	struct UiElement2D *hudStructPtr;
	struct GameTracker *gGT;

	gGT = sdata->gGT;
	hudStructPtr = data.hudStructPtr[gGT->numPlyrCurrGame - 1];

	UI_DrawNumRelic(hudStructPtr[0xE].x + 0x10, hudStructPtr[0xE].y - 10);
	UI_DrawNumKey(hudStructPtr[0xF].x + 0x10, hudStructPtr[0xF].y - 10);
	UI_DrawNumTrophy(hudStructPtr[0x10].x + 0x10, hudStructPtr[0x10].y - 10);
}
