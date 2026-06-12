#include <common.h>

#ifdef CTR_NATIVE
static void MainInit_InitVisMemBspListNodes(struct VisMem *visMem, struct mesh_info *mesh)
{
	if (mesh == NULL || mesh->bspRoot == NULL)
		return;

	for (int playerIndex = 0; playerIndex < 4; playerIndex++)
	{
		struct VisMemBspListNode *bspList = visMem->bspList[playerIndex];

		if (bspList == NULL)
			continue;

		for (int bspIndex = 0; bspIndex < mesh->numBspNodes; bspIndex++)
		{
			// NOTE(aalhendi): Native 226 reads the retained BSP pointer; RenderLists only rewrites the link word.
			bspList[bspIndex].next = NULL;
			bspList[bspIndex].bsp = &mesh->bspRoot[bspIndex];
		}
	}
}
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003af84-0x8003b008 for the retail path.
void MainInit_VisMem(struct GameTracker *gGT)
{
	struct VisMem *visMem = gGT->level1->visMem;
	gGT->visMem1 = visMem;

	if (visMem == NULL)
		return;

	for (int i = 0; i < gGT->numPlyrCurrGame; i++)
	{
		visMem->visLeafSrc[i] = NULL;
		visMem->visFaceSrc[i] = NULL;
		visMem->visOVertSrc[i] = NULL;
		visMem->visSCVertSrc[i] = NULL;
	}

#ifdef CTR_NATIVE
	MainInit_InitVisMemBspListNodes(visMem, gGT->level1->ptr_mesh_info);
#endif
}
