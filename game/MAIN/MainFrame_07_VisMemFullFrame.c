#include <common.h>

static void MainFrame_ReplacePackedVisList(int *dst, void *src, int byteCount)
{
	u32 srcWord = (u32)src;

	if ((srcWord & 1) == 0)
	{
		memcpy(dst, src, byteCount);
		return;
	}

	CTR_unknownMaybeThunk1(dst, (void *)(srcWord & ~(u32)3));
}

static void MainFrame_OrPackedVisList(int *dst, void *src, int byteCount)
{
	u32 srcWord = (u32)src;

	if ((srcWord & 1) == 0)
	{
		CTR_unknownMaybeThunk3(dst, src, byteCount);
		return;
	}

	CTR_unknownMaybeThunk2(dst, (void *)(srcWord & ~(u32)3));
}

static int MainFrame_VisMemHasQuad(const int *visFaceList, const struct QuadBlock *quad, const struct mesh_info *mesh)
{
	int quadIndex = (int)(quad - mesh->ptrQuadBlockArray);

	return (visFaceList[quadIndex >> 5] & (1 << (quadIndex & 0x1f))) != 0;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80035684-0x800357b8, unnamed in syms926.
static void MainFrame_VisMemAddDriverPVS(struct GameTracker *gGT, int playerIndex)
{
	struct Driver *driver = gGT->drivers[playerIndex];
	struct mesh_info *mesh = gGT->level1->ptr_mesh_info;
	struct QuadBlock *quad = driver->underDriver;
	struct PVS *pvs;

	if (quad == NULL)
		return;

	pvs = quad->pvs;
	if (pvs == NULL)
		return;

	if (pvs->visLeafSrc != NULL)
	{
		MainFrame_OrPackedVisList(gGT->visMem1->visLeafList[playerIndex], pvs->visLeafSrc, ((mesh->numBspNodes + 0x1f) >> 5) << 2);
	}

	if (pvs->visFaceSrc != NULL)
	{
		MainFrame_OrPackedVisList(gGT->visMem1->visFaceList[playerIndex], pvs->visFaceSrc, ((mesh->numQuadBlock + 0x1f) >> 5) << 2);
	}
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800357b8-0x80035d30.
void MainFrame_VisMemFullFrame(struct GameTracker *gGT, struct Level *level)
{
	struct VisMem *visMem;
	struct mesh_info *mesh;
	int playerIndex;

	visMem = gGT->visMem1;
	if (visMem == NULL)
		return;

	if (level == NULL)
		return;

	if (gGT->numPlyrCurrGame == 0)
		return;

	mesh = level->ptr_mesh_info;

	for (playerIndex = 0; playerIndex < gGT->numPlyrCurrGame; playerIndex++)
	{
		struct CameraDC *camDC = &gGT->cameraDC[playerIndex];
		struct Driver *driver = gGT->drivers[playerIndex];
		struct QuadBlock *driverQuad = driver->underDriver;
		struct PVS *driverPVS = NULL;

		if (driverQuad != NULL)
			driverPVS = driverQuad->pvs;

		camDC->flags &= ~0x4000;

		if (camDC->visLeafSrc == NULL)
		{
			if ((driverPVS != NULL) && (driverPVS->visLeafSrc != NULL))
			{
				visMem->visLeafSrc[playerIndex] = driverPVS->visLeafSrc;
				MainFrame_ReplacePackedVisList(visMem->visLeafList[playerIndex], driverPVS->visLeafSrc, ((mesh->numBspNodes + 0x1f) >> 5) << 2);
			}
		}
		else if (visMem->visLeafSrc[playerIndex] != camDC->visLeafSrc)
		{
			visMem->visLeafSrc[playerIndex] = camDC->visLeafSrc;
			MainFrame_ReplacePackedVisList(visMem->visLeafList[playerIndex], camDC->visLeafSrc, ((mesh->numBspNodes + 0x1f) >> 5) << 2);
		}

		if (camDC->visFaceSrc == NULL)
		{
			if ((driverPVS != NULL) && (driverPVS->visFaceSrc != NULL))
			{
				visMem->visFaceSrc[playerIndex] = driverPVS->visFaceSrc;
				MainFrame_ReplacePackedVisList(visMem->visFaceList[playerIndex], driverPVS->visFaceSrc, ((mesh->numQuadBlock + 0x1f) >> 5) << 2);
			}
		}
		else if (visMem->visFaceSrc[playerIndex] != camDC->visFaceSrc)
		{
			visMem->visFaceSrc[playerIndex] = camDC->visFaceSrc;
			MainFrame_ReplacePackedVisList(visMem->visFaceList[playerIndex], camDC->visFaceSrc, ((mesh->numQuadBlock + 0x1f) >> 5) << 2);

			if ((driverPVS == NULL) || (driverPVS->visLeafSrc == NULL) || (driverPVS->visFaceSrc == NULL) || (driverPVS->visInstSrc == NULL) ||
			    MainFrame_VisMemHasQuad(visMem->visFaceList[playerIndex], driverQuad, mesh))
			{
				camDC->flags &= ~0x2000;
			}
			else
			{
				camDC->flags |= 0x2000;
			}

			if ((camDC->flags & 0x2000) != 0)
			{
				MainFrame_VisMemAddDriverPVS(gGT, playerIndex);
				camDC->flags |= 0x4000;
			}
		}

		if ((camDC->flags & 0x5000) == 0x1000)
		{
			MainFrame_VisMemAddDriverPVS(gGT, playerIndex);
		}

		if ((camDC->cameraMode == 0) && ((camDC->flags & 0x2000) != 0) && (driverPVS != NULL) && (driverPVS->visInstSrc != NULL))
		{
			camDC->visInstSrc = driverPVS->visInstSrc;
		}

		if ((level->configFlags & 4) == 0)
		{
			if (visMem->visOVertSrc[playerIndex] != camDC->visOVertSrc)
			{
				visMem->visOVertSrc[playerIndex] = camDC->visOVertSrc;

				if (camDC->visOVertSrc != NULL)
				{
					MainFrame_ReplacePackedVisList(visMem->visOVertList[playerIndex], camDC->visOVertSrc, ((level->numWaterVertices + 0x1f) >> 5) << 2);
				}
				else
				{
					memcpy(visMem->visOVertList[playerIndex], level->unk5, ((level->numWaterVertices + 0x1f) >> 5) << 2);
				}
			}
			else if (visMem->visOVertSrc[playerIndex] == NULL)
			{
				memcpy(visMem->visOVertList[playerIndex], level->unk5, ((level->numWaterVertices + 0x1f) >> 5) << 2);
			}
		}
		else
		{
			if (visMem->visSCVertSrc[playerIndex] != camDC->visSCVertSrc)
			{
				visMem->visSCVertSrc[playerIndex] = camDC->visSCVertSrc;

				if (camDC->visSCVertSrc != NULL)
				{
					MainFrame_ReplacePackedVisList(visMem->visSCVertList[playerIndex], camDC->visSCVertSrc, ((level->numSCVert + 0x1f) >> 5) << 2);
				}
				else
				{
					memcpy(visMem->visSCVertList[playerIndex], level->unk_170, ((level->numSCVert + 0x1f) >> 5) << 2);
				}
			}
			else if (visMem->visSCVertSrc[playerIndex] == NULL)
			{
				memcpy(visMem->visSCVertList[playerIndex], level->unk_170, ((level->numSCVert + 0x1f) >> 5) << 2);
			}
		}
	}
}
