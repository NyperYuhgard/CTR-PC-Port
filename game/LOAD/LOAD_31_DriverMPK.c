#include <common.h>
#ifdef CTR_NATIVE
#include <platform/native_netplay.h>
#endif

static void (*const LOAD_DriverMPK_SetPointer)(struct LoadQueueSlot *) = (void (*)(struct LoadQueueSlot *))-2;

#ifdef CTR_NATIVE
/* Saved model+charId pairs from individual BI_RACERMODELHI loads for
 * remote players. LibraryOfModels_Clear in state 5 wipes gGT->modelPtr[],
 * so we save the pointers here and re-register them after the clear. */
#define MAX_SAVED_MODELS 256
static struct Model *s_savedModels[MAX_SAVED_MODELS];
static int s_savedModelCharIds[MAX_SAVED_MODELS];
static int s_savedModelCount;

/* Netplay: callback for loading BI_RACERMODELHI (individual high-res model)
 * for a remote player's character. Extracts the Model pointer from the
 * loaded file and saves it together with the character ID for later
 * registration in gGT->modelPtr[]. The HI model files all have id = -1,
 * so we use charId as the array index instead. */
static void LOAD_Callback_RemoteModel(struct LoadQueueSlot *lqs)
{
        struct Model *m = (struct Model *)lqs->ptrDestination;
        int charId = lqs->subfileIndex - BI_RACERMODELHI;
        fprintf(stdout, "[Netplay] LOAD_Callback_RemoteModel: subfile=%d charId=%d m=%p name='%s'\n",
                lqs->subfileIndex, charId, (void*)m, m ? m->name : "NULL"); fflush(stdout);
        if (m != NULL && charId >= 0 && charId < 16 && s_savedModelCount < MAX_SAVED_MODELS)
        {
                s_savedModels[s_savedModelCount] = m;
                s_savedModelCharIds[s_savedModelCount] = charId;
                s_savedModelCount++;
                fprintf(stdout, "[Netplay] LOAD_Callback_RemoteModel: SAVED at slot %d (charId=%d modelPtr[%d])\n",
                        s_savedModelCount - 1, charId, charId); fflush(stdout);
        }
}

/* Re-register remote-player models after LibraryOfModels_Clear wiped
 * gGT->modelPtr[]. Called from state 5 right after
 * LOAD_GlobalModelPtrs_MPK. HI model files have id = -1, so we use
 * the saved charId as the array index. */
void Netplay_RestoreDriverModels(struct GameTracker *gGT)
{
        int arrSize = sizeof(gGT->modelPtr) / sizeof(gGT->modelPtr[0]);
        fprintf(stdout, "[Netplay] Netplay_RestoreDriverModels: s_savedModelCount=%d, arrSize=%d\n",
                s_savedModelCount, arrSize); fflush(stdout);
        for (int i = 0; i < s_savedModelCount; i++)
        {
                struct Model *m = s_savedModels[i];
                int charId = s_savedModelCharIds[i];
                fprintf(stdout, "[Netplay] Netplay_RestoreDriverModels[%d]: charId=%d m=%p name='%s'\n",
                        i, charId, (void*)m, m ? m->name : "NULL"); fflush(stdout);
                if (m != NULL && charId >= 0 && charId < arrSize)
                {
                        gGT->modelPtr[charId] = m;
                        fprintf(stdout, "[Netplay] Netplay_RestoreDriverModels[%d]: wrote to modelPtr[%d] = %p\n",
                                i, charId, (void*)m); fflush(stdout);
                }
        }
        s_savedModelCount = 0;
}
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003282c-0x80032b50.
int LOAD_DriverMPK(struct BigHeader *bigfile, int levelLOD, void (*callback)(struct LoadQueueSlot *))
{
        int i;
        int gameMode1;

        struct GameTracker *gGT = sdata->gGT;
        gameMode1 = gGT->gameMode1;

        int lastFileIndexMPK;

#ifdef CTR_NATIVE
        /* Netplay: load player 0's 1P arcade pack for icons/DecalGlobal,
         * then load individual BI_RACERMODELHI files for each unique
         * remote character so they are visible. levelLOD is forced to 1
         * in LOAD_44_TenStages. */
        if (g_NetplayRacing)
        {
                int n = gGT->numPlyrCurrGame;

                /* Player 0 (local) gets the full pack — needed for ptrMPK,
                 * icons, DecalGlobal, and the local character's model. */
                lastFileIndexMPK = BI_1PARCADEPACK + data.characterIDs[0];

                /* Remote players (1..N-1): load individual HI model files.
                 * Each is much smaller than a full pack (~30KB vs ~295KB)
                 * and stores its Model at offset 0 of the file data
                 * (after the 4-byte ptrMapOffset header). */
                {
                        int seenChar[16];
                        memset(seenChar, 0, sizeof(seenChar));
                        seenChar[data.characterIDs[0]] = 1;

                        for (i = 1; i < n && i < 8; i++)
                        {
                                int charId = data.characterIDs[i];
                                if (charId < 0 || charId >= 16)
                                        continue;
                                if (seenChar[charId])
                                        continue;
                                seenChar[charId] = 1;

                                LOAD_AppendQueue(bigfile, LT_GETADDR,
                                                 BI_RACERMODELHI + charId,
                                                 NULL, LOAD_Callback_RemoteModel);
                        }
                }

                goto QueueLastPack;
        }
#endif

        // 3P/4P
        if (levelLOD - 3U < 2)
        {
                for (i = 0; i < 3; i++)
                {
                        // low lod CTR model
                        LOAD_AppendQueue(bigfile, LT_GETADDR, BI_RACERMODELLOW + data.characterIDs[i], &data.driverModelExtras[i], LOAD_DriverMPK_SetPointer);
                }

                // load 4P MPK of fourth player
                lastFileIndexMPK = BI_4PARCADEPACK + data.characterIDs[3];
        }

        else if (levelLOD == 1)
        {
                if ((gameMode1 & (TIME_TRIAL | MAIN_MENU)) == TIME_TRIAL)
                        goto LoadHighAndPack;

                if (
                    // adv/cutscene mpk when we just need text from MPK
                    ((gameMode1 & (GAME_CUTSCENE | ADVENTURE_ARENA)) != 0) ||

                    // credits
                    ((gGT->gameMode2 & CREDITS) != 0) ||

                    // adventure character select
                    (gGT->levelID == ADVENTURE_GARAGE))
                {
                        lastFileIndexMPK = BI_ADVENTUREPACK + data.characterIDs[0];
                        goto QueueLastPack;
                }

                if ((gameMode1 & ADVENTURE_BOSS) != 0)
                        goto LoadHighAndPack;

                if (
                    // If you are in Adventure cup
                    ((gameMode1 & ADVENTURE_CUP) != 0) &&

                    // purple gem cup
                    (gGT->cup.cupID == 4))
                {
                        // high lod model
                        LOAD_AppendQueue(bigfile, LT_GETADDR, BI_RACERMODELHI + data.characterIDs[0], &data.driverModelExtras[0], LOAD_DriverMPK_SetPointer);

                        // pack of four AIs with bosses
                        LOAD_AppendQueue(bigfile, LT_GETADDR, BI_2PARCADEPACK + 7, NULL, callback);

                        data.characterIDs[1] = RIPPER_ROO;
                        data.characterIDs[2] = PAPU_PAPU;
                        data.characterIDs[3] = KOMODO_JOE;
                        data.characterIDs[4] = PINSTRIPE;

                        return sdata->ptrMPK;
                }

                if ((gameMode1 & (TIME_TRIAL | MAIN_MENU)) != MAIN_MENU)
                {
#ifdef CTR_NATIVE
                        /* Netplay: do NOT call LOAD_Robots1P. That function
                         * overwrites characterIDs[1..7] with AI character IDs,
                         * but in netplay characterIDs[1..N-1] are the REMOTE
                         * peers' characters (set via CHARACTER_SELECT packets).
                         * Overwriting them would make the local machine load
                         * the wrong kart models for remote players and the
                         * AIs would be spawned even though MainInit_Drivers
                         * skips them. */
                        if (!g_NetplayRacing)
#endif
                                LOAD_Robots1P(data.characterIDs[0]);
                }

                // arcade mpk
                lastFileIndexMPK = BI_1PARCADEPACK + data.characterIDs[0];
        }

        else if ((levelLOD == 8) || ((gameMode1 & TIME_TRIAL) != 0))
        {
        LoadHighAndPack:
                // Do NOT switch the order to optimize Relic,
                // if HI+IDs[1] and PACK+IDs[0] is loaded,
                // then mask-grab breaks for all characters
                // on Hot Air Skyway (except Crash Bandicoot)

                // Load Player 1 [0]
                LOAD_AppendQueue(bigfile, LT_GETADDR, BI_RACERMODELHI + data.characterIDs[0], &data.driverModelExtras[0], LOAD_DriverMPK_SetPointer);

                // Load boss or ghost [1]
                lastFileIndexMPK = BI_TIMETRIALPACK + data.characterIDs[1];
        }

        // else if (levelLOD == 2)
        else
        {
                // med models
                for (i = 0; i < 2; i++)
                {
                        // med lod CTR model
                        LOAD_AppendQueue(bigfile, LT_GETADDR, BI_RACERMODELMED + data.characterIDs[i], &data.driverModelExtras[i], LOAD_DriverMPK_SetPointer);
                }

                LOAD_Robots2P(bigfile, data.characterIDs[0], data.characterIDs[1], callback);
                return sdata->ptrMPK;
        }

QueueLastPack:
        LOAD_AppendQueue(bigfile, LT_GETADDR, lastFileIndexMPK, NULL, callback);
        return sdata->ptrMPK;
}
