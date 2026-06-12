#include <common.h>
#include <platform/native_mods.h>

static int s_modsMenuSelectedIndex = 0;

void MM_Mods_Init(void)
{
	s_modsMenuSelectedIndex = 0;
}

void MM_Mods_MenuProc(struct RectMenu *menu)
{
	struct GameTracker *gGT = sdata->gGT;
	u_long *ot = &gGT->backBuffer->otMem.startPlusFour[3];
	int modCount;
	int i;
	int y;

	if (menu->rowSelected == -1)
	{
		menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		s_modsMenuSelectedIndex = 0;
		return;
	}

	modCount = NativeMods_GetModCount();
	y = 0x6c;

	DecalFont_DrawLineOT(sdata->lngStrings[0x014], 0x180, 0x40, FONT_BIG, WHITE, ot);

	if (modCount == 0)
	{
		DecalFont_DrawLineOT("No mods found.", 0x180, y, FONT_SMALL, ORANGE, ot);
		DecalFont_DrawLineOT("Create a mod folder in mods/", 0x180, y + 14, FONT_SMALL, ORANGE, ot);
	}
	else
	{
		for (i = 0; i < modCount && i < 8; i++)
		{
			const struct NativeModInfo *mod = NativeMods_GetMod(i);
			int color;
			char statusText[16];
			int isSelected = (i == s_modsMenuSelectedIndex);

			if (mod == NULL)
				break;

			if (isSelected)
			{
				color = WHITE;
				DecalFont_DrawLineOT(">", 0x160, y, FONT_SMALL, PLAYER_YELLOW, ot);
			}
			else
			{
				color = ORANGE;
			}

			if (mod->enabled)
				strcpy(statusText, "[ON] ");
			else
				strcpy(statusText, "[OFF]");

			DecalFont_DrawLineOT((char *)mod->name, 0x170, y, FONT_SMALL, color, ot);
			DecalFont_DrawLineOT(statusText, 0x280, y, FONT_SMALL, mod->enabled ? TINY_GREEN : RED, ot);

			y += 16;
		}

		if (modCount > 8)
		{
			DecalFont_DrawLineOT("...", 0x180, y, FONT_SMALL, ORANGE, ot);
			DecalFont_DrawLineOT("(+ more mods)", 0x180, y + 14, FONT_SMALL, ORANGE, ot);
			y += 28;
		}
	}

	DecalFont_DrawLineOT("UP/DOWN: Navigate  X: Toggle  TRIANGLE: Back", 0x180, 0x110, FONT_SMALL, ORANGE, ot);

	if (modCount == 0)
		return;

	if (sdata->buttonTapPerPlayer[0] & (BTN_UP))
	{
		s_modsMenuSelectedIndex = (s_modsMenuSelectedIndex - 1 + modCount) % modCount;
		OtherFX_Play(0, 1);
	}
	else if (sdata->buttonTapPerPlayer[0] & (BTN_DOWN))
	{
		s_modsMenuSelectedIndex = (s_modsMenuSelectedIndex + 1) % modCount;
		OtherFX_Play(0, 1);
	}
	else if (sdata->buttonTapPerPlayer[0] & (BTN_CROSS))
	{
		if (s_modsMenuSelectedIndex < modCount)
		{
			NativeMods_ToggleMod(s_modsMenuSelectedIndex);
			OtherFX_Play(1, 1);
		}
	}
	else if (sdata->buttonTapPerPlayer[0] & (BTN_TRIANGLE | BTN_SQUARE))
	{
		MM_JumpTo_Title_Returning();
	}
}