#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80032c24-0x80032d30.
int LOAD_GetBigfileIndex(u32 levelID, int lod, int fileIndexInGroup)
{
	if (levelID < NITRO_COURT)
		return BI_ARCADETRACKS + levelID * 8 + sdata->levBigLodIndex[lod - 1] + fileIndexInGroup;

	if ((u32)(levelID - NITRO_COURT) < 7)
		return BI_BATTLETRACKS + (levelID - NITRO_COURT) * 8 + sdata->levBigLodIndex[lod - 1] + fileIndexInGroup;

	if ((u32)(levelID - INTRO_RACE_TODAY) < 9)
		return BI_CUTSCENES_INTRO + (levelID - INTRO_RACE_TODAY) * 3 + fileIndexInGroup;

	if ((u32)(levelID - OXIDE_ENDING) < 2)
		return BI_CUTSCENES_OUTRO + (levelID - OXIDE_ENDING) * 2 + fileIndexInGroup;

	if (levelID == ADVENTURE_GARAGE)
		return BI_MAINMENUFILE + 2 + fileIndexInGroup;

	if (levelID == NAUGHTY_DOG_CRATE)
		return BI_NDBOX + fileIndexInGroup;

	if ((u32)(levelID - CREDITS_CRASH) < 20)
		return BI_CREDITS + (levelID - CREDITS_CRASH) * 3 + fileIndexInGroup;

	if (levelID == MAIN_MENU_LEVEL)
		return BI_MAINMENUFILE + fileIndexInGroup;

	if (levelID == SCRAPBOOK)
		return BI_SCRAPBOOK + fileIndexInGroup;

	return BI_ADVENTUREHUB + (levelID - GEM_STONE_VALLEY) * 3 + fileIndexInGroup;
}
