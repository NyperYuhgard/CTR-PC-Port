#include <common.h>
#ifdef CTR_NATIVE
#include <platform/native_netplay.h>
#endif

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
        int visIndex = playerIndex;

#ifdef CTR_NATIVE
        /* Netplay: visMem arrays are at index 0 for the local player,
         * regardless of the player's actual ID. See MainFrame_VisMemFullFrame. */
        if (g_NetplayRacing)
                visIndex = 0;
#endif

        if (quad == NULL)
                return;

        pvs = quad->pvs;
        if (pvs == NULL)
                return;

        if (pvs->visLeafSrc != NULL)
        {
                MainFrame_OrPackedVisList(gGT->visMem1->visLeafList[visIndex], pvs->visLeafSrc, ((mesh->numBspNodes + 0x1f) >> 5) << 2);
        }

        if (pvs->visFaceSrc != NULL)
        {
                MainFrame_OrPackedVisList(gGT->visMem1->visFaceList[visIndex], pvs->visFaceSrc, ((mesh->numQuadBlock + 0x1f) >> 5) << 2);
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
                struct CameraDC *camDC;
                struct Driver *driver;
                struct QuadBlock *driverQuad;
                struct PVS *driverPVS = NULL;
                int visMemIndex;

#ifdef CTR_NATIVE
                /* Netplay: only the local player has a camera and visMem
                 * list. Remote players don't have cameraDC initialized and
                 * their visLeafList[visMemIndex] is NULL, which would crash
                 * MainFrame_ReplacePackedVisList. Skip them.
                 *
                 * The local player's visibility data is stored at visMemIndex=0
                 * because the rendering code (RenderLists_Init1P2P etc.)
                 * always reads visLeafList[0] in 1P mode (which is what
                 * netplay uses locally, regardless of which player ID we are). */
                if (g_NetplayRacing && playerIndex != Netplay_GetLocalPlayerId())
                        continue;

                /* In netplay, store the local player's vis data at index 0
                 * (not at playerIndex) because the render code uses [0]. */
                visMemIndex = g_NetplayRacing ? 0 : playerIndex;
#else
                visMemIndex = playerIndex;
#endif

                driver = gGT->drivers[playerIndex];

                if (driver == NULL)
                        continue;

                camDC = &gGT->cameraDC[playerIndex];
                driverQuad = driver->underDriver;

                if (driverQuad != NULL)
                        driverPVS = driverQuad->pvs;

                camDC->flags &= ~0x4000;

                if (camDC->visLeafSrc == NULL)
                {
                        if ((driverPVS != NULL) && (driverPVS->visLeafSrc != NULL))
                        {
                                visMem->visLeafSrc[visMemIndex] = driverPVS->visLeafSrc;
                                MainFrame_ReplacePackedVisList(visMem->visLeafList[visMemIndex], driverPVS->visLeafSrc, ((mesh->numBspNodes + 0x1f) >> 5) << 2);
                        }
                }
                else if (visMem->visLeafSrc[visMemIndex] != camDC->visLeafSrc)
                {
                        visMem->visLeafSrc[visMemIndex] = camDC->visLeafSrc;
                        MainFrame_ReplacePackedVisList(visMem->visLeafList[visMemIndex], camDC->visLeafSrc, ((mesh->numBspNodes + 0x1f) >> 5) << 2);
                }

                if (camDC->visFaceSrc == NULL)
                {
                        if ((driverPVS != NULL) && (driverPVS->visFaceSrc != NULL))
                        {
                                visMem->visFaceSrc[visMemIndex] = driverPVS->visFaceSrc;
                                MainFrame_ReplacePackedVisList(visMem->visFaceList[visMemIndex], driverPVS->visFaceSrc, ((mesh->numQuadBlock + 0x1f) >> 5) << 2);
                        }
                }
                else if (visMem->visFaceSrc[visMemIndex] != camDC->visFaceSrc)
                {
                        visMem->visFaceSrc[visMemIndex] = camDC->visFaceSrc;
                        MainFrame_ReplacePackedVisList(visMem->visFaceList[visMemIndex], camDC->visFaceSrc, ((mesh->numQuadBlock + 0x1f) >> 5) << 2);

                        if ((driverPVS == NULL) || (driverPVS->visLeafSrc == NULL) || (driverPVS->visFaceSrc == NULL) || (driverPVS->visInstSrc == NULL) ||
                            MainFrame_VisMemHasQuad(visMem->visFaceList[visMemIndex], driverQuad, mesh))
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
                        if (visMem->visOVertSrc[visMemIndex] != camDC->visOVertSrc)
                        {
                                visMem->visOVertSrc[visMemIndex] = camDC->visOVertSrc;

                                if (camDC->visOVertSrc != NULL)
                                {
                                        MainFrame_ReplacePackedVisList(visMem->visOVertList[visMemIndex], camDC->visOVertSrc, ((level->numWaterVertices + 0x1f) >> 5) << 2);
                                }
                                else
                                {
                                        memcpy(visMem->visOVertList[visMemIndex], level->unk5, ((level->numWaterVertices + 0x1f) >> 5) << 2);
                                }
                        }
                        else if (visMem->visOVertSrc[visMemIndex] == NULL)
                        {
                                memcpy(visMem->visOVertList[visMemIndex], level->unk5, ((level->numWaterVertices + 0x1f) >> 5) << 2);
                        }
                }
                else
                {
                        if (visMem->visSCVertSrc[visMemIndex] != camDC->visSCVertSrc)
                        {
                                visMem->visSCVertSrc[visMemIndex] = camDC->visSCVertSrc;

                                if (camDC->visSCVertSrc != NULL)
                                {
                                        MainFrame_ReplacePackedVisList(visMem->visSCVertList[visMemIndex], camDC->visSCVertSrc, ((level->numSCVert + 0x1f) >> 5) << 2);
                                }
                                else
                                {
                                        memcpy(visMem->visSCVertList[visMemIndex], level->unk_170, ((level->numSCVert + 0x1f) >> 5) << 2);
                                }
                        }
                        else if (visMem->visSCVertSrc[visMemIndex] == NULL)
                        {
                                memcpy(visMem->visSCVertList[visMemIndex], level->unk_170, ((level->numSCVert + 0x1f) >> 5) << 2);
                        }
                }
        }
}
