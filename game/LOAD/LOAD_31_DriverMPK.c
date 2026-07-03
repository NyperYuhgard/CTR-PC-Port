#include <common.h>
#ifdef CTR_NATIVE
#include <platform/native_netplay.h>
#endif

static void (*const LOAD_DriverMPK_SetPointer)(struct LoadQueueSlot *) = (void (*)(struct LoadQueueSlot *))-2;

#ifdef CTR_NATIVE
/* Saved model pointers from non-last 1P arcade packs.
 * LibraryOfModels_Clear in state 5 wipes gGT->modelPtr[], so we save
 * the actual model pointers here and re-register them after the clear +
 * LOAD_GlobalModelPtrs_MPK. We must save model pointers (not pack
 * addresses) because the MEMPACK region at ptrMPK+4 gets overwritten
 * by subsequent loading operations before state 5 runs. */
#define MAX_SAVED_MODELS 256
static struct Model *s_savedModels[MAX_SAVED_MODELS];
static int s_savedModelCount;

/* Netplay: callback for non-last 1P arcade packs. Registers the pack's
 * models immediately in gGT->modelPtr[] via LibraryOfModels_Store, so
 * that characters from earlier packs are visible even though ptrMPK
 * ends up pointing to the last pack. Also saves model pointers so they
 * survive LibraryOfModels_Clear in state 5. */
static void LOAD_Callback_DriverModels_Netplay(struct LoadQueueSlot *lqs)
{
        struct GameTracker *gGT = sdata->gGT;
        int ptrMPK = (int)lqs->ptrDestination;
        if (ptrMPK != 0)
        {
                struct Model **arr = (struct Model **)((u32)ptrMPK + 4);
                if (arr != NULL && *arr != NULL)
                {
                        /* Register ALL models until NULL terminator (use -1).
                         * The 1P arcade pack's PLYROBJECTLIST includes both
                         * item/world models at low indices and the character
                         * model at higher indices. With -1, we capture them
                         * all instead of only the first 8. */
                        LibraryOfModels_Store(gGT, -1, arr);

                        /* Save each model pointer so it survives the
                         * LibraryOfModels_Clear in state 5. The model objects
                         * themselves live within the MEMPACK allocation and
                         * remain valid. */
                        for (int mi = 0; ; mi++)
                        {
                                struct Model *m = arr[mi];
                                if (m == NULL)
                                        break;
                                if (s_savedModelCount < MAX_SAVED_MODELS)
                                        s_savedModels[s_savedModelCount++] = m;
                        }
                }
        }
        BFDBG_PRINTF("Callback_DriverModels_Netplay: registered models from pack at %p", lqs->ptrDestination);
}

/* Re-register models from non-last 1P arcade packs after
 * LibraryOfModels_Clear wiped gGT->modelPtr[]. Called from state 5
 * of LOAD_TenStages right after LOAD_GlobalModelPtrs_MPK. */
void Netplay_RestoreDriverModels(struct GameTracker *gGT)
{
        int arrSize = sizeof(gGT->modelPtr) / sizeof(gGT->modelPtr[0]);
        for (int i = 0; i < s_savedModelCount; i++)
        {
                struct Model *m = s_savedModels[i];
                if (m != NULL && m->id >= 0 && m->id < arrSize)
                        gGT->modelPtr[m->id] = m;
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
        /* Netplay: load the 1P arcade pack for EVERY connected peer's
         * character, not just player 0. Each 1P pack contains 8 kart
         * models (the picked character + 7 AIs), but the AIs differ per
         * pack — so to guarantee that peer N's character has a model
         * loaded, we must load peer N's pack too.
         *
         * With 2 peers that's 2 × ~295KB = ~590KB, which fits in the
         * expanded MEMPACK (~4.6MB usables after 24-bit clamping).
         *
         * The LAST pack in the queue uses the real callback so ptrMPK
         * gets set for LOAD_GlobalModelPtrs_MPK / icons / DecalGlobal.
         * The earlier packs use LOAD_DriverMPK_SetPointer (sentinel -2)
         * which just stores the pointer without invoking a callback —
         * their models get registered later via PLYROBJECTLIST only
         * for the last pack. To register ALL packs' models, we also
         * store each pack's address in driverModelExtras so
         * LOAD_GlobalModelPtrs_MPK picks them up.
         *
         * levelLOD is forced to 1 in LOAD_44_TenStages. */
        if (g_NetplayRacing)
        {
                int n = gGT->numPlyrCurrGame;
                int lastPeer = -1;
                int seenChar[16];
                int j;

                /* Track which characters we've already loaded to avoid
                 * loading the same pack twice (e.g. two peers picked
                 * the same character). */
                memset(seenChar, 0, sizeof(seenChar));

                for (i = 0; i < n && i < 8; i++)
                {
                        int charId = data.characterIDs[i];
                        if (charId < 0 || charId >= 16)
                                continue;
                        if (seenChar[charId])
                                continue;
                        seenChar[charId] = 1;
                        lastPeer = i;
                }

                /* Re-iterate and queue each unique pack. All except the
                 * last use the sentinel pointer setter; the last uses
                 * the real callback so ptrMPK gets set. */
                {
                        int queued = 0;
                        int totalUnique = 0;
                        for (i = 0; i < n && i < 8; i++)
                        {
                                int charId = data.characterIDs[i];
                                if (charId < 0 || charId >= 16)
                                        continue;
                                /* Count uniques (cheap re-scan) */
                                {
                                        int isUnique = 1;
                                        int k;
                                        for (k = 0; k < i; k++)
                                        {
                                                if (data.characterIDs[k] == charId)
                                                {
                                                        isUnique = 0;
                                                        break;
                                                }
                                        }
                                        if (!isUnique) continue;
                                }
                                totalUnique++;
                        }

                        for (i = 0; i < n && i < 8; i++)
                        {
                                int charId = data.characterIDs[i];
                                if (charId < 0 || charId >= 16)
                                        continue;
                                /* Skip duplicates */
                                {
                                        int isUnique = 1;
                                        int k;
                                        for (k = 0; k < i; k++)
                                        {
                                                if (data.characterIDs[k] == charId)
                                                {
                                                        isUnique = 0;
                                                        break;
                                                }
                                        }
                                        if (!isUnique) continue;
                                }

                                queued++;
                                if (queued < totalUnique)
                                {
                                        /* Not the last — use our custom callback that
                                         * registers this pack's models immediately. */
                                        LOAD_AppendQueue(bigfile, LT_GETADDR,
                                                         BI_1PARCADEPACK + charId,
                                                         NULL, LOAD_Callback_DriverModels_Netplay);
                                }
                                else
                                {
                                        /* Last unique pack — use real callback so
                                         * ptrMPK gets set for icons/DecalGlobal. */
                                        lastFileIndexMPK = BI_1PARCADEPACK + charId;
                                        goto QueueLastPack;
                                }
                        }

                        /* If no peers had valid characters, fall back to player 0 */
                        lastFileIndexMPK = BI_1PARCADEPACK + data.characterIDs[0];
                        goto QueueLastPack;
                }
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
