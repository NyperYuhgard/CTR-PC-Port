#include <common.h>

#if defined(CTR_NATIVE) && defined(CTR_INTERNAL)
#include <platform/native_perf.h>
#include <platform/native_replay_scheduler.h>
#include <platform/native_savestate.h>
#endif
#ifdef CTR_NATIVE
#include <platform/native_mods.h>
#include <platform/native_netplay.h>
#endif

#if defined(CTR_NATIVE) && defined(CTR_INTERNAL)
static struct NativePerfFrameInfo MainPerf_FrameInfo(struct GameTracker *gGT)
{
        struct NativePerfFrameInfo info;

        info.frameCounter = sdata->frameCounter;
        info.timer = gGT->timer;
        info.levelID = gGT->levelID;
        info.gameMode1 = gGT->gameMode1;
        info.loadingStage = sdata->Loading.stage;
        info.boolDemoMode = gGT->boolDemoMode;
        info.numPlyrCurrGame = gGT->numPlyrCurrGame;
        info.elapsedTimeMS = gGT->elapsedTimeMS;
        info.vsyncTillFlip = sdata->vsyncTillFlip;
        info.vSync_between_drawSync = gGT->vSync_between_drawSync;
        info.frameTimer_VsyncCallback = gGT->frameTimer_VsyncCallback;

        return info;
}

static struct NativeReplaySchedulerFrameInfo MainReplayScheduler_FrameInfo(struct GameTracker *gGT)
{
        struct NativeReplaySchedulerFrameInfo info;

        info.frameTimer = gGT->frameTimer_VsyncCallback;
        info.frameCounter = sdata->frameCounter;
        info.timer = gGT->timer;
        info.framesInThisLEV = gGT->framesInThisLEV;
        info.elapsedTimeMS = gGT->elapsedTimeMS;
        info.msInThisLEV = gGT->msInThisLEV;
        info.elapsedEventTime = gGT->elapsedEventTime;
        info.mainGameState = sdata->mainGameState;
        info.loadingStage = sdata->Loading.stage;
        info.levelID = gGT->levelID;
        info.mixRandomNumber = (u32)sdata->randomNumber;
        info.audioRNG = sdata->audioRNG;
        info.deadcoed0 = (u32)gGT->deadcoed_struct.unk1;
        info.deadcoed1 = (u32)gGT->deadcoed_struct.unk2;
        info.advRng0 = (u32)sdata->const_0x30215400;
        info.advRng1 = (u32)sdata->const_0x493583fe;

        return info;
}
#endif

// NOTE(aalhendi): PSX path ASM-verified NTSC-U 926 0x8003c58c-0x8003cf7c.
#ifdef CTR_NATIVE
u32 CTR_Main(void)
#else
u32 main(void)
#endif
{
        u32 AddBitsConfig0;
        u32 RemBitsConfig0;
        u32 AddBitsConfig8;
        u32 RemBitsConfig8;
        int iVar8;
        u32 gameMode1;
        u32 gameMode2;
        u32 uVar12;

        struct GameTracker *gGT;
        gGT = sdata->gGT;

        struct GamepadSystem *gGS;
        gGS = sdata->gGamepads;

        // NOTE(aalhendi): Retail main calls __main before the state loop. Native has
        // no linked __main body, so keep this as a CTR_NATIVE-only divergence.
#ifndef CTR_NATIVE
        __main();
#endif

        do
        {
#ifndef CTR_NATIVE
                // wont happen under normal conditions
                if (sdata->mainGameState == 5)
                {
                        MainKillGame_StopCTR();
                        return 0;
                }
#endif

                LOAD_NextQueuedFile();
                // NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003c5d0-0x8003c5dc for per-frame XA pause handling.
                CDSYS_XAPauseAtEnd();

                switch (sdata->mainGameState)
                {
                // Initialize Game (happens once)
                case 0:
                        StateZero();
                        break;

                // Happens on first frame that loading ends
                case 1:

                        ElimBG_Deactivate(gGT);

                        MainStats_RestartRaceCountLoss();
                        // NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003c9f8-0x8003ca04 for load-complete voiceline reset.
                        Voiceline_ClearTimeStamp();

                        // Disable End-Of-Race menu
                        gGT->gameMode1 &= ~END_OF_RACE;

                        if (gGT->levelID == MAIN_MENU_LEVEL)
                        {
                                if (RaceFlag_IsFullyOffScreen() != 0)
                                        RaceFlag_SetFullyOnScreen();
                        }

                        else
                        {
                                if (RaceFlag_IsFullyOnScreen() != 0)
                                        RaceFlag_BeginTransition(2);
                        }

                        DropRain_Reset(gGT);
                        GAMEPROG_GetPtrHighScoreTrack();
#ifdef CTR_NATIVE
                        fprintf(stdout, "[Netplay] Calling MainInit_FinalizeInit (g_NetplayRacing=%d)\n", g_NetplayRacing); fflush(stdout);
#endif
                        MainInit_FinalizeInit(gGT);
                        GAMEPAD_GetNumConnected(gGS);

                        sdata->boolSoundPaused = 0;
                        // NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003caa4-0x8003cab4 for load-complete engine audio init.
                        VehBirth_EngineAudio_AllPlayers();

                        // 9 = intro cutscene
                        // 10 = traffic lights
                        // 11 = racing

                        // Arcade-Style track starts with intro cutscene
                        uVar12 = 9;

                        if (
                            // If Level ID is less than 18, it's one of the race tracks
                            (gGT->levelID < NITRO_COURT) || (
                                                                // Battle-Style track starts with traffic lights
                                                                uVar12 = 10,
                                                                // Level ID >= 18 and < 23
                                                                // Battle tracks
                                                                gGT->levelID - NITRO_COURT < 7))
                        {
                                Audio_SetState_Safe(uVar12);
                        }
                        sdata->mainGameState = 3;
                        gGT->clockEffectEnabled &= 0xfffe;
                        break;

                // Reset stage, reset music
                case 2:
                        Audio_SetState_Safe(1);
                        MEMPACK_PopState();

                        // ignore threads, because we PopState,
                        // so the threadpool will reset anyway
                        LevInstDef_RePack(gGT->level1->ptr_mesh_info, 0);

                        sdata->mainGameState = 1;
                        break;

                // Main Gameplay Update
                // Makes up all normal interaction with the game
                case 3:

                        // if loading, or gameplay interrupted
                        if (sdata->Loading.stage != -1)
                        {
                                if ((RaceFlag_IsFullyOnScreen() == 1) || (gGT->levelID == NAUGHTY_DOG_CRATE) || (sdata->pause_state != 0))
                                {
                                        gGT->gameMode1 |= LOADING;
                                }

                                iVar8 = sdata->Loading.stage;

                                // elapsed milliseconds per frame, locked 32 here
                                // impacts speed of flag wave during "loading...", but does not impact speed of flying text
                                gGT->elapsedTimeMS = 32;

                                // if loading VLC
                                if (iVar8 == -6)
                                {
                                        // if VLC is not loaded, quit
                                        // we know when it's done from a load callback
                                        if (sdata->bool_IsLoaded_VlcTable != 1)
                                                break;

                                        // if == 1, finish the loading
                                        goto FinishLoading;
                                }

                                // if restarting race
                                if (iVar8 == -5)
                                {
                                        if (RaceFlag_IsFullyOnScreen() == 1)
                                        {
                                                // reinitialize world,
                                                // does not reinitialize pools
                                                sdata->mainGameState = 2;

                                                // no loading, and no interruption
                                                sdata->Loading.stage = -1;

                                                // Turn off the "Loading..." flag
                                                gGT->gameMode1 &= ~LOADING;
                                                break;
                                        }

                                        // if not fully on-screen, do not BREAK,
                                        // keep rendering the scene
                                }

                                // if waiting for checkered flag to cover screen,
                                // right before loading the next requested level
                                else if (iVar8 == -4)
                                {
                                        RemBitsConfig8 = sdata->Loading.OnBegin.RemBitsConfig8;
                                        AddBitsConfig8 = sdata->Loading.OnBegin.AddBitsConfig8;
                                        RemBitsConfig0 = sdata->Loading.OnBegin.RemBitsConfig0;
                                        AddBitsConfig0 = sdata->Loading.OnBegin.AddBitsConfig0;

                                        if (RaceFlag_IsFullyOnScreen() == 1)
                                        {
                                                sdata->Loading.OnBegin.AddBitsConfig0 = 0;
                                                sdata->Loading.OnBegin.RemBitsConfig0 = 0;
                                                sdata->Loading.OnBegin.AddBitsConfig8 = 0;
                                                sdata->Loading.OnBegin.RemBitsConfig8 = 0;

                                                gameMode2 = gGT->gameMode2;

                                                gGT->hudFlags &= 0xf7;

                                                gameMode1 = gGT->gameMode1;
                                                gGT->gameMode2 = gameMode2 | AddBitsConfig8;
                                                gGT->gameMode1 = gameMode1 | AddBitsConfig0;
                                                gGT->gameMode1 = (gameMode1 | AddBitsConfig0) & ~RemBitsConfig0;
                                                gGT->gameMode2 = (gameMode2 | AddBitsConfig8) & ~RemBitsConfig8;

                                                MainRaceTrack_StartLoad(sdata->Loading.Lev_ID_To_Load);
                                        }

                                        else if (RaceFlag_IsFullyOffScreen() == 1)
                                                RaceFlag_BeginTransition(1);

                                        // do not BREAK,
                                        // keep rendering the scene
                                }

                                // if something is being loaded
                                else
                                {
                                        sdata->Loading.stage = LOAD_TenStages(gGT, iVar8, sdata->ptrBigfile1);

                                        // If just finished loading stage 9
                                        if (sdata->Loading.stage == -2)
                                        {
                                                if ((gGT->levelID == MAIN_MENU_LEVEL) || (gGT->levelID == SCRAPBOOK))
                                                {
                                                        MainLoadVLC();

                                                        // start loading VLC (scroll up to iVar8 == -6)
                                                        sdata->Loading.stage = -6;
                                                        break;
                                                }

                                        FinishLoading:
                                                // loading is finished,
                                                // initialize world and pools,
                                                // remove LOADING... flag from gGT
                                                sdata->Loading.stage = -1;
                                                sdata->mainGameState = 1;
                                                gGT->gameMode1 &= ~LOADING;
                                                break;
                                        }

                                        // else, do not BREAK,
                                        // keep rendering the scene
                                        // which is the checkered flag
                                }
                        }

                        // =========== Main Game Loop ======================

                        if ((
                                // Check value of traffic lights
                                (-960 < gGT->trafficLightsTimer) &&
                                // if not drawing intro race cutscene and if not paused
                                ((gGT->gameMode1 & (START_OF_RACE | PAUSE_ALL)) == 0)
#ifdef CTR_NATIVE
                                // Netplay: wait until ALL machines (local + every active peer) finish loading
                                && (!g_NetplayRacing || Netplay_IsEveryoneLoaded())
#endif
                                ) &&
                            (
                                // amount of milliseconds on Traffic Lights - elapsed milliseconds per frame, ~32
                                iVar8 = gGT->trafficLightsTimer - gGT->elapsedTimeMS,
                                // decrease amount of time on Traffic Lights
                                gGT->trafficLightsTimer = iVar8,
                                // if countdown has gone down far enough for traffic lights to go off-screen
                                iVar8 < -960))
                        {
                                // set a floor value, so countdown can't go farther negative
                                gGT->trafficLightsTimer = 0xfffffc40;
                        }

                        // frame counter
#ifdef CTR_NATIVE
                        static int s_60fpsFcToggle = 0;
                        int advanceFrame = 1;
                        if (IS_INTERP_60FPS)
                        {
                                // Interpolated mode: frame advances every other call
                                static int s_interpFrameToggle = 0;
                                advanceFrame = (s_interpFrameToggle ^= 1);
                        }
                        if (!IS_NATIVE_60FPS || (s_60fpsFcToggle ^= 1))
                        {
                                if (advanceFrame)
                                        sdata->frameCounter++;
                        }
#else
                        sdata->frameCounter++;
#endif

                        // Process all gamepad input
#if defined(CTR_NATIVE) && defined(CTR_INTERNAL)
                        {
                                struct NativeReplaySchedulerFrameInfo replayFrameInfo = MainReplayScheduler_FrameInfo(gGT);

                                if (NativeReplayScheduler_BeginFrame(&replayFrameInfo) != 0)
                                        return 0;
                                NativeSaveState_BeginFrame();
                                gGT = sdata->gGT;
                                gGS = sdata->gGamepads;
                        }
                        {
                                struct NativePerfFrameInfo perfFrameInfo = MainPerf_FrameInfo(gGT);

                                NativePerf_BeginFrame(&perfFrameInfo);
                        }
#endif
                        GAMEPAD_ProcessAnyoneVars(gGS);

#ifdef CTR_NATIVE
                        NativeMods_CallHook(NATIVE_MOD_HOOK_ON_INPUT);

                        // Netplay: sync gamepad state across all players during a race (N-player general)
                        {
                                if (g_NetplayRacing && sdata->Loading.stage == -1)
                                {
                                        int localId = Netplay_GetLocalPlayerId();
                                        int playerCount = gGT->numPlyrCurrGame;
                                        if (playerCount > NETPLAY_MAX_PLAYERS)
                                                playerCount = NETPLAY_MAX_PLAYERS;
                                        if (playerCount < 1)
                                                playerCount = 1;

                                        // Per-player input accumulators (initialize to neutral)
                                        u32 pHeld[NETPLAY_MAX_PLAYERS];
                                        u32 pTapped[NETPLAY_MAX_PLAYERS];
                                        u32 pReleased[NETPLAY_MAX_PLAYERS];
                                        s16 pLX[NETPLAY_MAX_PLAYERS], pLY[NETPLAY_MAX_PLAYERS];
                                        s16 pRX[NETPLAY_MAX_PLAYERS], pRY[NETPLAY_MAX_PLAYERS];
                                        int p;
                                        for (p = 0; p < NETPLAY_MAX_PLAYERS; p++)
                                        {
                                                pHeld[p] = 0;
                                                pTapped[p] = 0;
                                                pReleased[p] = 0;
                                                pLX[p] = 0x80;
                                                pLY[p] = 0x80;
                                                pRX[p] = 0x80;
                                                pRY[p] = 0x80;
                                        }

                                        // Local physical controller (always gamepad[0])
                                        struct GamepadBuffer *physPad = &gGS->gamepad[0];

                                        // True local tap/release computed from physical held state
                                        // (GAMEPAD_ProcessTapRelease would use a contaminated prev frame on clients).
                                        static u32 s_physicalPrevHeld = 0;
                                        u32 physicalCurr = physPad->buttonsHeldCurrFrame;
                                        u32 trueTapped = ~s_physicalPrevHeld & physicalCurr;
                                        u32 trueReleased = s_physicalPrevHeld & ~physicalCurr;
                                        s_physicalPrevHeld = physicalCurr;

                                        u32 frameNum = sdata->frameCounter;

                                        // 1) Local player: capture + send
                                        if (localId < NETPLAY_MAX_PLAYERS)
                                        {
                                                pHeld[localId] = physicalCurr;
                                                pTapped[localId] = trueTapped;
                                                pReleased[localId] = trueReleased;
                                                pLX[localId] = physPad->stickLX;
                                                pLY[localId] = physPad->stickLY;
                                                pRX[localId] = physPad->stickRX;
                                                pRY[localId] = physPad->stickRY;

                                                Netplay_SendGamepadState(frameNum,
                                                                         pHeld[localId], pTapped[localId],
                                                                         pReleased[localId],
                                                                         pLX[localId], pLY[localId],
                                                                         pRX[localId], pRY[localId]);
                                        }

                                        // 2) Receive remote inputs for this frame
                                        {
                                                struct NetplayInput inputs[NETPLAY_MAX_PLAYERS];
                                                int count = Netplay_ReceiveInputsForFrame(inputs, NETPLAY_MAX_PLAYERS, frameNum);
                                                int got[NETPLAY_MAX_PLAYERS];
                                                int k;
                                                for (k = 0; k < NETPLAY_MAX_PLAYERS; k++) got[k] = 0;

                                                for (k = 0; k < count; k++)
                                                {
                                                        u8 rid = inputs[k].playerId;
                                                        if (rid < NETPLAY_MAX_PLAYERS && rid != localId)
                                                        {
                                                                pHeld[rid] = inputs[k].buttonsHeld;
                                                                pTapped[rid] = inputs[k].buttonsTapped;
                                                                /* Remote item use comes from ITEM_USE
                                                                 * packets, NOT from predicted input.
                                                                 * Zero BTN_CIRCLE to prevent phantom
                                                                 * weapon fires when the engine processes
                                                                 * the remote player's circle tap. */
                                                                pTapped[rid] &= ~BTN_CIRCLE;
                                                                pReleased[rid] = inputs[k].buttonsReleased;
                                                                pLX[rid] = inputs[k].stickLX;
                                                                pLY[rid] = inputs[k].stickLY;
                                                                pRX[rid] = inputs[k].stickRX;
                                                                pRY[rid] = inputs[k].stickRY;
                                                                got[rid] = 1;
                                                        }
                                                }

                                                // Fallback: if no fresh input for a remote player this frame,
                                                // reuse the latest known held state but drop tap/release (turbo fix).
                                                for (k = 0; k < NETPLAY_MAX_PLAYERS; k++)
                                                {
                                                        if (k == localId) continue;
                                                        if (!got[k])
                                                        {
                                                                struct NetplayInput latest;
                                                                Netplay_GetLatestRemoteInput((u8)k, &latest);
                                                                if (latest.frameNum != 0)
                                                                {
                                                                        pHeld[k] = latest.buttonsHeld;
                                                                        pTapped[k] = 0;
                                                                        pReleased[k] = 0;
                                                                        pLX[k] = latest.stickLX;
                                                                        pLY[k] = latest.stickLY;
                                                                        pRX[k] = latest.stickRX;
                                                                        pRY[k] = latest.stickRY;
                                                                }
                                                        }
                                                }
                                        }

                                        // 3) Write per-player state into gGS->gamepad[0..N-1]
                                        for (p = 0; p < playerCount; p++)
                                        {
                                                gGS->gamepad[p].buttonsHeldCurrFrame = pHeld[p];
                                                gGS->gamepad[p].buttonsTapped = pTapped[p];
                                                gGS->gamepad[p].buttonsReleased = pReleased[p];
                                                gGS->gamepad[p].stickLX = pLX[p];
                                                gGS->gamepad[p].stickLY = pLY[p];
                                                gGS->gamepad[p].stickRX = pRX[p];
                                                gGS->gamepad[p].stickRY = pRY[p];

                                                // Update menu input variables
                                                sdata->buttonTapPerPlayer[p] = pTapped[p];
                                                sdata->buttonHeldPerPlayer[p] = pHeld[p];
                                        }

                                        // Union of all player taps/holds
                                        {
                                                u32 anyTap = 0, anyHold = 0;
                                                for (p = 0; p < playerCount; p++)
                                                {
                                                        anyTap |= pTapped[p];
                                                        anyHold |= pHeld[p];
                                                }
                                                sdata->AnyPlayerTap = anyTap;
                                                sdata->AnyPlayerHold = anyHold;
                                        }

                                        // 4) Pause sync: if remote player paused/unpaused, mirror it
                                        if (Netplay_ConsumeRemotePause())
                                        {
                                                gGT->gameMode1 |= PAUSE_ALL;
                                        }
                                        if (Netplay_ConsumeRemoteUnpause())
                                        {
                                                gGT->gameMode1 &= ~PAUSE_ALL;
                                        }

                                        // 5) State sync (position/velocity correction) for ALL remote drivers
                                        {
                                                u32 frameNum2 = sdata->frameCounter;

                                                // Send local driver state every 5 frames (or immediately if resync requested)
                                                static u32 s_lastStateSend = 0;
                                                if (g_NetplayStateRequested)
                                                {
                                                        g_NetplayStateRequested = 0;
                                                        s_lastStateSend = 0;
                                                }
                                                if (frameNum2 - s_lastStateSend >= 5 || s_lastStateSend == 0)
                                                {
                                                        s_lastStateSend = frameNum2;
                                                        if (localId < NETPLAY_MAX_PLAYERS)
                                                        {
                                                                struct Driver *localDriver = gGT->drivers[localId];
                                                                if (localDriver != NULL)
                                                                {
                                                                        struct NetplayStatePayload st;
                                                                        st.frameNum = frameNum2;
                                                                        st.posX = localDriver->posCurr.x;
                                                                        st.posY = localDriver->posCurr.y;
                                                                        st.posZ = localDriver->posCurr.z;
                                                                        st.rotX = localDriver->rotCurr.x;
                                                                        st.rotY = localDriver->rotCurr.y;
                                                                        st.rotZ = localDriver->rotCurr.z;
                                                                        st.rotW = localDriver->rotCurr.w;
                                                                        st.speed = localDriver->speed;
                                                                        st.kartState = (u8)localDriver->kartState;
                                                                        st.lapIndex = localDriver->lapIndex;
                                                                        st.heldItemID = (u8)localDriver->heldItemID;
                                                                        st.numHeldItems = (u8)localDriver->numHeldItems;
                                                                        st.actionsFlagSet = localDriver->actionsFlagSet;
                                                                        st.velX = localDriver->velocity.x;
                                                                        st.velY = localDriver->velocity.y;
                                                                        st.velZ = localDriver->velocity.z;
                                                                        /* Extra rotation fields. Without these the remote
                                                                         * kart's rotation drifts because the client's local
                                                                         * angle/turnAngleCurr keep advancing from inputs
                                                                         * while we only snap rotCurr (which the engine
                                                                         * overwrites next frame via
                                                                         *   rotCurr.y = unk3D4[0] + angle + turnAngleCurr). */
                                                                        st.angle = localDriver->angle;
                                                                        st.turnAngleCurr = localDriver->turnAngleCurr;
                                                                        st.unk3D4_0 = localDriver->unk3D4[0];
                                                                        /* v2.1: numWumpas + noItemTimer.
                                                                         * numWumpas is needed by the HUD to pick between
                                                                         * TNT vs Nitro icon (and Potion/Shield powered-up
                                                                         * variants). noItemTimer drives the weapon-flicker
                                                                         * animation, which would otherwise desync and look
                                                                         * weird (icon disappearing at different times). */
                                                                        st.numWumpas = (u8)localDriver->numWumpas;
                                                                        st.noItemTimer = (s16)localDriver->noItemTimer;
                                                                        Netplay_SendStatePacket(&st);

                                                                        // Broadcast race finish when local player finishes
                                                                        if ((localDriver->actionsFlagSet & 0x2000000) && !Netplay_AnyRemoteFinished())
                                                                        {
                                                                                Netplay_MarkRemoteFinished((u8)localId);
                                                                                {
                                                                                        u8 payload = (u8)localId;
                                                                                        Netplay_BroadcastPacket(NETPLAY_PACKET_FINISHED, 1, &payload);
                                                                                }
                                                                        }
                                                                }
                                                        }
                                                }

                                                // Apply received remote state corrections for every non-local driver
                                                for (p = 0; p < playerCount; p++)
                                                {
                                                        if (p == localId) continue;
                                                        {
                                                                struct NetplayStatePayload remoteState;
                                                                if (Netplay_DequeueState((u8)p, &remoteState))
                                                                {
                                                                        Netplay_ClearState((u8)p);
                                                                        struct Driver *remoteDriver = gGT->drivers[p];
                                                                        if (remoteDriver != NULL)
                                                                        {
                                                                                s32 dx = remoteState.posX - remoteDriver->posCurr.x;
                                                                                s32 dy = remoteState.posY - remoteDriver->posCurr.y;
                                                                                s32 dz = remoteState.posZ - remoteDriver->posCurr.z;
                                                                                u32 distSq = (u32)(dx*dx + dy*dy + dz*dz);

                                                                                /* Large desync: snap. Small drift: lerp 20%. */
                                                                                int snap = (distSq > 0x1000000);
                                                                                if (snap)
                                                                                {
                                                                                        remoteDriver->posCurr.x = remoteState.posX;
                                                                                        remoteDriver->posCurr.y = remoteState.posY;
                                                                                        remoteDriver->posCurr.z = remoteState.posZ;
                                                                                        remoteDriver->velocity.x = remoteState.velX;
                                                                                        remoteDriver->velocity.y = remoteState.velY;
                                                                                        remoteDriver->velocity.z = remoteState.velZ;
                                                                                        remoteDriver->speed = remoteState.speed;
                                                                                }
                                                                                else
                                                                                {
                                                                                        remoteDriver->posCurr.x += dx / 5;
                                                                                        remoteDriver->posCurr.y += dy / 5;
                                                                                        remoteDriver->posCurr.z += dz / 5;
                                                                                        /* Also nudge velocity toward target so the
                                                                                         * lerp doesn't fight against the engine's
                                                                                         * own integration. */
                                                                                        remoteDriver->velocity.x += (remoteState.velX - remoteDriver->velocity.x) / 5;
                                                                                        remoteDriver->velocity.y += (remoteState.velY - remoteDriver->velocity.y) / 5;
                                                                                        remoteDriver->velocity.z += (remoteState.velZ - remoteDriver->velocity.z) / 5;
                                                                                        remoteDriver->speed += (remoteState.speed - remoteDriver->speed) / 5;
                                                                                }

                                                                                /* === Rotation sync ===
                                                                                 * CTR angles are 12-bit modular (0x1000 = 360°).
                                                                                 * A direct snap causes the kart to spin when
                                                                                 * crossing the 0xFFF -> 0x000 boundary. We use
                                                                                 * the shortest angular difference and lerp by
                                                                                 * 1/3 (faster than position because rotation
                                                                                 * mismatches are visually jarring).
                                                                                 *
                                                                                 * We also sync the underlying angle,
                                                                                 * turnAngleCurr and unk3D4[0] — without these,
                                                                                 * the engine recomputes rotCurr.y next frame
                                                                                 * from its own (stale) values and undoes our
                                                                                 * snap. */
                                                                                {
                                                                                        s16 target_rotX = remoteState.rotX;
                                                                                        s16 target_rotY = remoteState.rotY;
                                                                                        s16 target_rotZ = remoteState.rotZ;
                                                                                        s16 target_rotW = remoteState.rotW;

                                                                                        if (snap)
                                                                                        {
                                                                                                remoteDriver->rotCurr.x = target_rotX;
                                                                                                remoteDriver->rotCurr.y = target_rotY;
                                                                                                remoteDriver->rotCurr.z = target_rotZ;
                                                                                                remoteDriver->rotCurr.w = target_rotW;
                                                                                        }
                                                                                        else
                                                                                        {
                                                                                                /* Shortest-path angular lerp for each axis.
                                                                                                 * diff is wrapped to [-0x800, +0x800) so we
                                                                                                 * always take the shorter way around, then
                                                                                                 * we apply 1/3 of it. */
                                                                                                s32 diffX = ((((s32)(target_rotX - remoteDriver->rotCurr.x)) + 0x800) & 0xFFF) - 0x800;
                                                                                                s32 diffY = ((((s32)(target_rotY - remoteDriver->rotCurr.y)) + 0x800) & 0xFFF) - 0x800;
                                                                                                s32 diffZ = ((((s32)(target_rotZ - remoteDriver->rotCurr.z)) + 0x800) & 0xFFF) - 0x800;
                                                                                                remoteDriver->rotCurr.x = (s16)((remoteDriver->rotCurr.x + diffX / 3) & 0xFFF);
                                                                                                remoteDriver->rotCurr.y = (s16)((remoteDriver->rotCurr.y + diffY / 3) & 0xFFF);
                                                                                                remoteDriver->rotCurr.z = (s16)((remoteDriver->rotCurr.z + diffZ / 3) & 0xFFF);
                                                                                                /* rotW is not modular — direct lerp */
                                                                                                remoteDriver->rotCurr.w = (s16)(remoteDriver->rotCurr.w + (target_rotW - remoteDriver->rotCurr.w) / 3);
                                                                                        }

                                                                                        /* Sync the underlying physics fields. Snap
                                                                                         * always (no lerp) because they're inputs to
                                                                                         * the next frame's rotCurr.y computation,
                                                                                         * so a partial lerp would just get
                                                                                         * overwritten. */
                                                                                        remoteDriver->angle = remoteState.angle;
                                                                                        remoteDriver->turnAngleCurr = remoteState.turnAngleCurr;
                                                                                        remoteDriver->unk3D4[0] = remoteState.unk3D4_0;

                                                                                        /* v2.1: numWumpas + noItemTimer.
                                                                                         * numWumpas is consumed by the HUD to pick
                                                                                         * between TNT vs Nitro icon (and powered-up
                                                                                         * Potion/Shield). Without this, the remote
                                                                                         * kart's weapon icon shows the wrong variant.
                                                                                         * noItemTimer drives weapon flicker — sync
                                                                                         * it so the icon doesn't blink out of sync. */
                                                                                        remoteDriver->numWumpas = (char)remoteState.numWumpas;
                                                                                        remoteDriver->noItemTimer = (s16)remoteState.noItemTimer;
                                                                                }

                                                                                remoteDriver->kartState = (char)remoteState.kartState;
                                                                                remoteDriver->lapIndex = remoteState.lapIndex;
                                                                                remoteDriver->heldItemID = (char)remoteState.heldItemID;
                                                                                remoteDriver->numHeldItems = (char)remoteState.numHeldItems;
                                                                                /* Mask out the fire-flag bit (0x8000) from the
                                                                                 * synced actionsFlagSet. This flag is a 1-frame
                                                                                 * "player tapped circle" signal that should ONLY
                                                                                 * be set via ITEM_USE packets, not via state sync.
                                                                                 * If we let state sync set it, the engine would
                                                                                 * re-fire the weapon every time a snapshot arrives
                                                                                 * that happens to capture the flag — causing the
                                                                                 * infinite-missile bug. */
                                                                                remoteDriver->actionsFlagSet =
                                                                                        (remoteDriver->actionsFlagSet & 0x8000) |
                                                                                        (remoteState.actionsFlagSet & ~0x8000u);

                                                                                if (remoteState.actionsFlagSet & 0x2000000)
                                                                                        Netplay_MarkRemoteFinished((u8)p);
                                                                        }
                                                                }
                                                        }
                                                }
                                        }

                                        // 6) Apply remote crate hits (use crateID for robust match)
                                        {
                                                struct NetplayCrateHit crateHit;
                                                while (Netplay_DequeueCrateHit(&crateHit))
                                                {
                                                        if (gGT->level1 != NULL && gGT->level1->ptrInstDefs != NULL)
                                                        {
                                                                int num = gGT->level1->numInstances;
                                                                int i;
                                                                for (i = 0; i < num; i++)
                                                                {
                                                                        struct Instance *inst = gGT->level1->ptrInstDefs[i].ptrInstance;
                                                                        if (inst == NULL) continue;

                                                                        /* Prefer crateID match when available; fall back to
                                                                         * position match for backward compat. */
                                                                        int match = 0;
                                                                        if (crateHit.crateID != 0)
                                                                        {
                                                                                u32 localID = Netplay_ComputeCrateID(
                                                                                        (int)gGT->levelID, i);
                                                                                if (localID == crateHit.crateID)
                                                                                        match = 1;
                                                                        }
                                                                        else if (inst->matrix.t[0] == crateHit.posX &&
                                                                                 inst->matrix.t[1] == crateHit.posY &&
                                                                                 inst->matrix.t[2] == crateHit.posZ)
                                                                        {
                                                                                match = 1;
                                                                        }

                                                                        if (match)
                                                                        {
                                                                                inst->scale[0] = 0;
                                                                                inst->scale[1] = 0;
                                                                                inst->scale[2] = 0;
                                                                                if (inst->thread != NULL)
                                                                                {
                                                                                        struct Crate *c = (struct Crate *)inst->thread->object;
                                                                                        if (c != NULL && c->cooldown == 0)
                                                                                                c->cooldown = 0x1e;
                                                                                }
                                                                                break;
                                                                        }
                                                                }
                                                        }
                                                }
                                        }

                                        // 6b) Apply remote item pickups (set heldItemID + numHeldItems
                                        // directly on the remote driver, bypassing the 5-frame state
                                        // snapshot interval). This fixes the "wrong item icon" desync.
                                        for (p = 0; p < playerCount; p++)
                                        {
                                                if (p == localId) continue;
                                                {
                                                        u8 itemId, numItems;
                                                        if (Netplay_DequeueItemPickup((u8)p, &itemId, &numItems))
                                                        {
                                                                struct Driver *remoteDriver = gGT->drivers[p];
                                                                if (remoteDriver != NULL)
                                                                {
                                                                        remoteDriver->heldItemID = (char)itemId;
                                                                        remoteDriver->numHeldItems = (char)numItems;
                                                                        /* Also reset item roll timer so the HUD shows
                                                                         * the icon immediately. */
                                                                        remoteDriver->itemRollTimer = 0;
                                                                }
                                                        }
                                                }
                                        }

                                        // 6c) Apply remote item use. Instead of calling
                                        // VehPickupItem_ShootNow directly (which caused an
                                        // infinite-fire bug because the engine's per-frame
                                        // logic re-triggered the shot), we set the fire flag
                                        // (0x8000) on the remote driver. The engine's own
                                        // VehPickupItem_ShootOnCirclePress will pick it up
                                        // next frame, fire the weapon ONCE, and clear the
                                        // flag. This is the natural code path and avoids
                                        // double-firing or infinite loops.
                                        for (p = 0; p < playerCount; p++)
                                        {
                                                if (p == localId) continue;
                                                {
                                                        u8 itemId;
                                                        if (Netplay_DequeueItemUse((u8)p, &itemId))
                                                        {
                                                                struct Driver *remoteDriver = gGT->drivers[p];
                                                                if (remoteDriver != NULL)
                                                                {
                                                                        /* Only fire if the remote driver
                                                                         * actually has an item. */
                                                                        if (remoteDriver->heldItemID != 0xF &&
                                                                            remoteDriver->heldItemID != 0x10)
                                                                        {
                                                                                /* Set the fire flag. The engine's
                                                                                 * VehPickupItem_ShootOnCirclePress
                                                                                 * (called from VehPhysProc per-frame
                                                                                 * logic) will see this, fire the
                                                                                 * weapon, and clear the flag — all
                                                                                 * in one frame, no loop. */
                                                                                remoteDriver->actionsFlagSet |= 0x8000;
                                                                        }
                                                                }
                                                        }
                                                }
                                        }

                                        // 6d) Consume RNG seed from host (clients only, once)
                                        {
                                                u32 seed, seedFrame;
                                                if (Netplay_ConsumeRngSeed(&seed, &seedFrame))
                                                {
                                                        sdata->randomNumber = (int)(seed & 0xFFFF);
                                                        fprintf(stdout, "[Netplay] Applied RNG seed 0x%08x at frame %u\n",
                                                                seed, seedFrame);
                                                        fflush(stdout);
                                                }
                                        }

                                        // 7) Checksum: compare each machine's simulation of the SAME remote driver.
                                        // For each non-local driver, compute our local checksum of it, send it
                                        // (with driverId in payload), and compare against the most recent remote
                                        // checksum we received for the same driverId.
                                        {
                                                static u32 s_lastChecksumSend = 0;
                                                u32 frameNum3 = sdata->frameCounter;
                                                if (frameNum3 - s_lastChecksumSend >= 30)
                                                {
                                                        s_lastChecksumSend = frameNum3;
                                                        for (p = 0; p < playerCount; p++)
                                                        {
                                                                if (p == localId) continue;
                                                                {
                                                                        struct Driver *remoteDriver = gGT->drivers[p];
                                                                        if (remoteDriver != NULL)
                                                                        {
                                                                                struct NetplayChecksumPayload cp;
                                                                                cp.frameNum = frameNum3;
                                                                                cp.driverId = (u8)p;
                                                                                cp.checksum = (u32)(remoteDriver->posCurr.x * 3 +
                                                                                                    remoteDriver->posCurr.y * 7 +
                                                                                                    remoteDriver->posCurr.z * 11 +
                                                                                                    remoteDriver->speed * 13 +
                                                                                                    remoteDriver->lapIndex * 17 +
                                                                                                    remoteDriver->actionsFlagSet * 19);
                                                                                Netplay_BroadcastPacket(NETPLAY_PACKET_CHECKSUM, sizeof(cp), &cp);

                                                                                // Compare with last remote checksum we received for this driver
                                                                                if (g_NetplayRemoteChecksumFrame != 0 &&
                                                                                    g_NetplayRemoteChecksumValue != cp.checksum)
                                                                                {
                                                                                        Netplay_BroadcastPacket(NETPLAY_PACKET_STATE_REQ, 0, NULL);
                                                                                        g_NetplayRemoteChecksumFrame = 0;
                                                                                }
                                                                        }
                                                                }
                                                        }
                                                }
                                        }

                                        // 8) Handle remote disconnect during race
                                        if (g_NetplayDisconnected)
                                        {
                                                g_NetplayDisconnected = 0;
                                                g_NetplayRacing = 0;
                                                MainRaceTrack_RequestLoad(MAIN_MENU_LEVEL);
                                        }

                                        // 9) Handle return-to-lobby signal from host
                                        if (Netplay_ConsumeReturnToLobby())
                                        {
                                                Netplay_ResetRaceState();
                                                MainRaceTrack_RequestLoad(MAIN_MENU_LEVEL);
                                        }
                                }
                        }
#endif

                        // Start new frame (ClearOTagR)
                        MainFrame_ResetDB(gGT);

                        if (
                            // If you're in Demo Mode
                            (gGT->boolDemoMode != 0) &&

                            (
                                // Turn off HUD
                                gGT->hudFlags &= 0xfe,
                                // if game is not loading
                                sdata->Loading.stage == -1))
                        {
                                // All this code is for the 30-second timer within Demo Mode
                                // To see 30-second timer in Main Menu, go to FUN_00001604 in 230.c
                                // pressing (or holding) any button sets it to zero

#ifdef CTR_NATIVE
{ static int s_60fpsDemoCountdown = 0; if (!IS_NATIVE_60FPS || (s_60fpsDemoCountdown ^= 1)) gGT->demoCountdownTimer--; }
#else
gGT->demoCountdownTimer--;
#endif

                                // check to see if time ran out
                                if (gGT->demoCountdownTimer < 1)
                                {
                                        // leave demo mode, go to main menu
                                        gGT->boolDemoMode = 0;
                                        gGT->numPlyrNextGame = 1;
                                        sdata->mainMenuState = 0;

                                LAB_8003ce08:
                                        MainRaceTrack_RequestLoad(MAIN_MENU_LEVEL);
                                }

                                // if time remains on the timer
                                else
                                {
                                        // if any button is pressed by anyone
                                        if (gGS->anyoneHeldCurr != 0)
                                        {
                                                // leave demo mode
                                                gGT->boolDemoMode = 0;
                                                goto LAB_8003ce08;
                                        }
                                }

                                // if numPlyrCurrGame is 1
                                if (gGT->numPlyrCurrGame == 1)
                                {
                                        // Draw text near top of screen
                                        uVar12 = 0x23;
                                }

                                // if this is multiplayer
                                else
                                {
                                        // draw text halfway to top of screen
                                        uVar12 = 100;
                                }

                                DecalFont_DrawMultiLine(sdata->lngStrings[LNG_DEMO_MODE_PRESS_ANY_BUTTON_TO_EXIT], 0x100, uVar12, 0x200, 2, 0xffff8000);
                        }

                        if ((gGT->gameMode1 & LOADING) == 0)
                        {
#ifdef CTR_NATIVE
                                // Interpolated mode: skip game logic on render-only frames
                                if (!IS_INTERP_60FPS || advanceFrame)
                                {
                                        NativeMods_CacheGameState();
                                        NativeMods_CallHook(NATIVE_MOD_HOOK_ON_UPDATE);
#if defined(CTR_NATIVE) && defined(CTR_INTERNAL)
                                        NativePerf_BeginScope(NATIVE_PERF_BUCKET_GAME_LOGIC);
#endif
                                        MainFrame_GameLogic(gGT, gGS);
#if defined(CTR_NATIVE) && defined(CTR_INTERNAL)
                                        NativePerf_EndScope(NATIVE_PERF_BUCKET_GAME_LOGIC);
#endif
                                }
#else
                                MainFrame_GameLogic(gGT, gGS);
#endif
                        }

#ifdef CTR_NATIVE
                        // Interpolated 60fps: save camera state after logic, interpolate on render-only frames
                        if (IS_INTERP_60FPS)
                        {
                                static s16 s_prevPushBufferPos[4][3];
                                static s16 s_prevPushBufferRot[4][3];
                                if (advanceFrame)
                                {
                                        // Save current pushbuffer state for interpolation
                                        int numPb = gGT->numPlyrCurrGame > 4 ? 4 : gGT->numPlyrCurrGame;
                                        if (numPb < 1) numPb = 1;
                                        for (int i = 0; i < numPb; i++)
                                        {
                                                s_prevPushBufferPos[i][0] = gGT->pushBuffer[i].pos[0];
                                                s_prevPushBufferPos[i][1] = gGT->pushBuffer[i].pos[1];
                                                s_prevPushBufferPos[i][2] = gGT->pushBuffer[i].pos[2];
                                                s_prevPushBufferRot[i][0] = gGT->pushBuffer[i].rot[0];
                                                s_prevPushBufferRot[i][1] = gGT->pushBuffer[i].rot[1];
                                                s_prevPushBufferRot[i][2] = gGT->pushBuffer[i].rot[2];
                                        }
                                }
                                else
                                {
                                        // Interpolate pushbuffer state between previous and current (50%)
                                        int numPb = gGT->numPlyrCurrGame > 4 ? 4 : gGT->numPlyrCurrGame;
                                        if (numPb < 1) numPb = 1;
                                        for (int i = 0; i < numPb; i++)
                                        {
                                                gGT->pushBuffer[i].pos[0] = (s16)(((int)s_prevPushBufferPos[i][0] + (int)gGT->pushBuffer[i].pos[0]) >> 1);
                                                gGT->pushBuffer[i].pos[1] = (s16)(((int)s_prevPushBufferPos[i][1] + (int)gGT->pushBuffer[i].pos[1]) >> 1);
                                                gGT->pushBuffer[i].pos[2] = (s16)(((int)s_prevPushBufferPos[i][2] + (int)gGT->pushBuffer[i].pos[2]) >> 1);
                                                gGT->pushBuffer[i].rot[0] = (s16)(((int)s_prevPushBufferRot[i][0] + (int)gGT->pushBuffer[i].rot[0]) >> 1);
                                                gGT->pushBuffer[i].rot[1] = (s16)(((int)s_prevPushBufferRot[i][1] + (int)gGT->pushBuffer[i].rot[1]) >> 1);
                                                gGT->pushBuffer[i].rot[2] = (s16)(((int)s_prevPushBufferRot[i][2] + (int)gGT->pushBuffer[i].rot[2]) >> 1);
                                                PushBuffer_SetMatrixVP(&gGT->pushBuffer[i]);
                                        }
                                }
                        }
#endif

                        // If you are in demo mode
                        if (gGT->boolDemoMode != '\0')
                        {
                                // Turn off HUD
                                gGT->hudFlags &= 0xfe;
                        }

                        // reset vsync calls between drawsync
                        gGT->vSync_between_drawSync = 0;


// NOTE(aalhendi): Native-only XA/request shim. Camera ownership belongs to CAM_ThTick.
#if defined(CTR_NATIVE)

                        if ((gGT->level1 != 0) && (gGT->levelID != MAIN_MENU_LEVEL) && (gGT->levelID != ADVENTURE_GARAGE) && (gGT->levelID != NAUGHTY_DOG_CRATE))
                        {
                                int held = gGS->gamepad[0].buttonsHeldCurrFrame;
#ifdef CTR_NATIVE
                                if (g_NetplayRacing)
                                        held |= gGS->gamepad[1].buttonsHeldCurrFrame;
#endif

                                if ((held & BTN_START) != 0)
                                {
                                        if ((gGT->gameMode1 & GAME_CUTSCENE) != 0)
                                        {
                                                MainRaceTrack_RequestLoad(MAIN_MENU_LEVEL);
                                        }
                                }
                        }
#endif


#ifdef CTR_NATIVE
                        Platform_BeginFrame();
                        NativeMods_CallHook(NATIVE_MOD_HOOK_ON_RENDER);
                        NativeMods_FlushDrawQueue();
#endif
#if defined(CTR_NATIVE) && defined(CTR_INTERNAL)
                        NativePerf_BeginScope(NATIVE_PERF_BUCKET_RENDER_FRAME);
#endif
#ifdef CTR_NATIVE
                        int savedNumPlyr = gGT->numPlyrCurrGame;
                        if (g_NetplayRacing)
                        {
                                gGT->numPlyrCurrGame = 1;
                        }
#endif
                        MainFrame_RenderFrame(gGT, gGS);
#ifdef CTR_NATIVE
                        if (g_NetplayRacing)
                        {
                                gGT->numPlyrCurrGame = savedNumPlyr;
                        }
#endif
#if defined(CTR_NATIVE) && defined(CTR_INTERNAL)
                        NativePerf_EndScope(NATIVE_PERF_BUCKET_RENDER_FRAME);
#endif
#ifdef CTR_NATIVE
                        Platform_EndFrame();
#endif


                        // if mask is talking in Adventure Hub
                        if (sdata->boolDraw3D_AdvMask != 0)
                        {
                                AH_MaskHint_Update();
                        }
#if defined(CTR_NATIVE) && defined(CTR_INTERNAL)
                        {
                                struct NativeReplaySchedulerFrameInfo replayFrameInfo = MainReplayScheduler_FrameInfo(gGT);

                                if (NativeReplayScheduler_EndFrame(&replayFrameInfo) != 0)
                                        return 0;
                        }
                        {
                                struct NativePerfFrameInfo perfFrameInfo = MainPerf_FrameInfo(gGT);

                                NativePerf_EndFrame(&perfFrameInfo);
                        }
#endif
                        break;

#ifndef CTR_NATIVE
                // In theory, this is left over from the demos,
                // which would "timeout" and restart after sitting idle
                case 4:

                        // erase all data past the
                        // last 3 bookmarks, if there
                        // that many exist
                        MEMPACK_PopState();
                        MEMPACK_PopState();
                        MEMPACK_PopState();

                        CTR_ErrorScreen(0, 0, 0);
                        Music_Stop();

                        // clear backup, destroy music, destroy all fx
                        howl_StopAudio(1, 1, 1);
                        Bank_DestroyAll();
                        howl_Disable();

                        GAMEPAD_SetMainMode();

                        // Set vsync to 2 FPS
                        VSync(30);

                        // reboot game
                        sdata->mainGameState = 0;
#endif
                }
        } while (true);
}

// NOTE(aalhendi): Source split of retail main's state-0 body
// 0x8003c614-0x8003c984.
// By separating this, it can be overwritten dynamically (oxide fix).
void StateZero()
{
        u16 *clockEffect;
        int vramSize;

        struct GameTracker *gGT;
        gGT = sdata->gGT;

        struct GamepadSystem *gGS;
        gGS = sdata->gGamepads;

// already zero, part of BSS
#if 0
        memset(gGT, 0, sizeof(struct GameTracker));
#endif

        // Set Video Mode to NTSC
        SetVideoMode(0);
        ResetCallback();

#define MEMPACK_SIZE 0x200000 // 2mb

        MEMPACK_Init(MEMPACK_SIZE);
        LOAD_InitCD();
        RaceFlag_SetFullyOffScreen();

        ResetGraph(0);
        SetGraphDebug(0);

        MainInit_VRAMClear();

        SetDispMask(1);

        SetDefDrawEnv(&gGT->db[0].drawEnv, 0, 0, 0x200, 0xd8);
        SetDefDrawEnv(&gGT->db[1].drawEnv, 0, 0x128, 0x200, 0xd8);
        SetDefDispEnv(&gGT->db[0].dispEnv, 0, 0x128, 0x200, 0xd8);
        SetDefDispEnv(&gGT->db[1].dispEnv, 0, 0, 0x200, 0xd8);

        gGT->db[0].dispEnv.screen.x = 0;
        gGT->db[0].dispEnv.screen.y = 0xc;
        gGT->db[0].dispEnv.screen.w = 0x100;
        gGT->db[0].dispEnv.screen.h = 0xd8;

        gGT->db[1].dispEnv.screen.x = 0;
        gGT->db[1].dispEnv.screen.y = 0xc;
        gGT->db[1].dispEnv.screen.w = 0x100;
        gGT->db[1].dispEnv.screen.h = 0xd8;

        gGT->db[0].drawEnv.isbg = 1;
        gGT->db[0].drawEnv.r0 = 0;
        gGT->db[0].drawEnv.g0 = 0;
        gGT->db[0].drawEnv.b0 = 0;

        gGT->db[1].drawEnv.isbg = 1;
        gGT->db[1].drawEnv.r0 = 0;
        gGT->db[1].drawEnv.g0 = 0;
        gGT->db[1].drawEnv.b0 = 0;

        // default number of lives in battle
        // this is left over from prototypes, useless in retail
        gGT->battleLifeLimit = 5;

        // 30 second counter?
        gGT->constVal_9000 = 9000;

        // set lap count to 3
        gGT->numLaps = 3;

        gGT->battleSetup.enabledWeapons |= 0x34de;
        gGT->numPlyrCurrGame = 1;
        gGT->numPlyrNextGame = 1;
        *(u32 *)&gGT->battleSetup.teamOfEachPlayer = 0x3020100;

        // traffic light countdown timer, set to negative one second
        gGT->trafficLightsTimer = 0xfffffc40;

        Timer_Init();
        DrawSyncCallback(&MainDrawCb_DrawSync);

        MEMCARD_InitCard();
        VSync(0);
        GAMEPAD_Init(gGS);
        VSync(0);
        GAMEPAD_GetNumConnected(gGS);

#ifdef CTR_NATIVE
#define BIGPATH "\\BIGFILE.BIG;1"
#else
#define BIGPATH rdata.s_PathTo_Bigfile
#endif

        // Get CD Position fo BIGFILE
        sdata->ptrBigfile1 = LOAD_ReadDirectory(BIGPATH);

// Defrag to save heap space,
// required because MEMPACK_Init moves heap
#if 0
        // NOTE(aalhendi): Retail main does not rewrite BIGFILE overlay sizes here.
        extern char RB_NewEndFile[4];

        // Dont load full overlay file, cut off the end
        struct BigEntry *firstEntry = BIG_GETENTRY(sdata->ptrBigfile1);
        firstEntry[231].size = 28 * 0x800;
        // firstEntry[231].size = (u32)RB_NewEndFile - (u32)OVR_Region3;
        // printf("Size: %08x\n", firstEntry[231].size);

        // Cut off Region1 overlays at 2 sectors (not 3),
        // This protects Region2 RAM so it is not overwritten
        // during Region1 disc streaming, saves loading time
        for (int i = 221; i <= 225; i++)
                firstEntry[i].size = 2 * 0x800;
#endif

        // English=1
        // PAL SCES02105 calls it multiple times
        LOAD_LangFile((int)sdata->ptrBigfile1, 1);
#ifdef CTR_NATIVE
        NativeMods_OnLanguageLoaded(sdata->lngStrings, sdata->numLngStrings);

        if (0x015 < sdata->numLngStrings)
                sdata->lngStrings[0x015] = "ONLINE";
#endif
        GAMEPROG_NewGame_OnBoot();
        gGT->overlayIndex_null_notUsed = 0;

        gGT->levelID = NAUGHTY_DOG_CRATE;
        // gGT->levelID = OXIDE_TRUE_ENDING;

        InitGeom();
        SetGeomOffset(0x100, 0x78); // width/2, height/2
        SetGeomScreen(0x140);       // "distance" to screen, alters FOV

        // NOTE(aalhendi): Retail calls 0x8006ae74 here; keep the verified depth
        // scale setup on every target.
        RenderBucket_InitDepthGTE();
        // NOTE(aalhendi): Retail bakes these authored matrix tables in place.
        Vector_BakeMatrixTable();

        gGT->swapchainIndex = 0;
        gGT->backBuffer = &gGT->db[0];

        gGT->overlayIndex_EndOfRace = 0xff;
        gGT->overlayIndex_LOD = 0xff;
        gGT->overlayIndex_Threads = 0xff;

        PutDispEnv(&gGT->db[1].dispEnv);
        PutDrawEnv(&gGT->db[1].drawEnv);
        DrawSync(0);

        // Load Intro TIM for "SCEA Presents" from VRAM file
        LOAD_VramFile(sdata->ptrBigfile1, 0x1fd, NULL, &vramSize, -1);
        MainInit_VRAMDisplay();

        // \SOUNDS\KART.HWL;1
        // NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003c8e0-0x8003c928 for startup HOWL/music/XA setup.
        howl_InitGlobals(data.kartHwlPath);

        VSyncCallback(MainDrawCb_Vsync);

        Music_SetIntro();
        CseqMusic_StopAll();
        CseqMusic_Start(0, 0, NULL, 0, 0);
        Music_Start(0);

        // "Start your engines, for Sony Computer..."
        CDSYS_XAPlay(CDSYS_XA_TYPE_EXTRA, 0x50);

        while (sdata->XA_State != 0)
        {
                // WARNING: Read-only address (ram, 0x8008d888) is written
                // NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003c940-0x8003c948 for startup XA pause polling.
#ifdef CTR_NATIVE
                // NOTE(aalhendi): Retail hardware interrupts keep XA/audio moving while
                // this loop spins. Native owns VBlank in VSync(), so pump it here.
                VSync(0);
#endif
                CDSYS_XAPauseAtEnd();
        }

        DecalGlobal_Clear(gGT);

        // This loads UI textures (shared.vrm)
        // This includes traffic lights, font, and more
        LOAD_VramFile(sdata->ptrBigfile1, 0x102, NULL, &vramSize, -1);

        sdata->mainGameState = 3;

        // start loading
        sdata->Loading.stage = 0;

        clockEffect = &gGT->clockEffectEnabled;
        gGT->gameMode1 |= LOADING;
        gGT->clockEffectEnabled = *clockEffect & 0xfffe;
}
