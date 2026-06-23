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
			        // Netplay: wait until both machines finish loading
			        && (!g_NetplayRacing || (g_NetplayLocalLoaded && g_NetplayRemoteLoaded))
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

			// Netplay: sync gamepad state between host and client during a race
			{
				if (g_NetplayRacing && sdata->Loading.stage == -1)
				{
					int localId = Netplay_GetLocalPlayerId();
					u32 hHeld = 0, hTapped = 0, hRel = 0;
					u32 cHeld = 0, cTapped = 0, cRel = 0;
					s16 hLX = 0x80, hLY = 0x80, hRX = 0x80, hRY = 0x80;
					s16 cLX = 0x80, cLY = 0x80, cRX = 0x80, cRY = 0x80;

					// gamepad[0] always has the local physical controller
					struct GamepadBuffer *physPad = &gGS->gamepad[0];

					u32 frameNum = gGT->frameTimer_VsyncCallback;

					if (localId == 0)
					{
						hHeld = physPad->buttonsHeldCurrFrame;
						hTapped = physPad->buttonsTapped;
						hRel = physPad->buttonsReleased;
						hLX = physPad->stickLX; hLY = physPad->stickLY;
						hRX = physPad->stickRX; hRY = physPad->stickRY;

						Netplay_SendGamepadState(frameNum,
						                         hHeld, hTapped, hRel,
						                         hLX, hLY, hRX, hRY);

						// Frame-matched receive: only consume inputs for the current frame
						struct NetplayInput inputs[4];
						int count = Netplay_ReceiveInputsForFrame(inputs, 4, frameNum);
						for (int i = 0; i < count; i++)
						{
							if (inputs[i].playerId == 1)
							{
								cHeld = inputs[i].buttonsHeld;
								cTapped = inputs[i].buttonsTapped;
								cRel = inputs[i].buttonsReleased;
								cLX = inputs[i].stickLX;
								cLY = inputs[i].stickLY;
								cRX = inputs[i].stickRX;
								cRY = inputs[i].stickRY;
							}
						}

						// If no matching frame arrived, use latest known remote input
						if (count == 0)
						{
							struct NetplayInput latest;
							Netplay_GetLatestRemoteInput(1, &latest);
							if (latest.frameNum != 0)
							{
								cHeld = latest.buttonsHeld;
								cTapped = latest.buttonsTapped;
								cRel = latest.buttonsReleased;
								cLX = latest.stickLX;
								cLY = latest.stickLY;
								cRX = latest.stickRX;
								cRY = latest.stickRY;
							}
						}
					}
					else
					{
						cHeld = physPad->buttonsHeldCurrFrame;
						cTapped = physPad->buttonsTapped;
						cRel = physPad->buttonsReleased;
						cLX = physPad->stickLX; cLY = physPad->stickLY;
						cRX = physPad->stickRX; cRY = physPad->stickRY;

						Netplay_SendGamepadState(frameNum,
						                         cHeld, cTapped, cRel,
						                         cLX, cLY, cRX, cRY);

						// Frame-matched receive: only consume inputs for the current frame
						struct NetplayInput inputs[4];
						int count = Netplay_ReceiveInputsForFrame(inputs, 4, frameNum);
						for (int i = 0; i < count; i++)
						{
							if (inputs[i].playerId == 0)
							{
								hHeld = inputs[i].buttonsHeld;
								hTapped = inputs[i].buttonsTapped;
								hRel = inputs[i].buttonsReleased;
								hLX = inputs[i].stickLX;
								hLY = inputs[i].stickLY;
								hRX = inputs[i].stickRX;
								hRY = inputs[i].stickRY;
							}
						}

						// If no matching frame arrived, use latest known remote input
						if (count == 0)
						{
							struct NetplayInput latest;
							Netplay_GetLatestRemoteInput(0, &latest);
							if (latest.frameNum != 0)
							{
								hHeld = latest.buttonsHeld;
								hTapped = latest.buttonsTapped;
								hRel = latest.buttonsReleased;
								hLX = latest.stickLX;
								hLY = latest.stickLY;
								hRX = latest.stickRX;
								hRY = latest.stickRY;
							}
						}
					}

					// On BOTH machines: gamepad[0] = host input, gamepad[1] = client input
					gGS->gamepad[0].buttonsHeldCurrFrame = hHeld;
					gGS->gamepad[0].buttonsTapped = hTapped;
					gGS->gamepad[0].buttonsReleased = hRel;
					gGS->gamepad[0].stickLX = hLX; gGS->gamepad[0].stickLY = hLY;
					gGS->gamepad[0].stickRX = hRX; gGS->gamepad[0].stickRY = hRY;

					gGS->gamepad[1].buttonsHeldCurrFrame = cHeld;
					gGS->gamepad[1].buttonsTapped = cTapped;
					gGS->gamepad[1].buttonsReleased = cRel;
					gGS->gamepad[1].stickLX = cLX; gGS->gamepad[1].stickLY = cLY;
					gGS->gamepad[1].stickRX = cRX; gGS->gamepad[1].stickRY = cRY;

					// Update menu input variables so both players can use menus
					sdata->buttonTapPerPlayer[0] = hTapped;
					sdata->buttonTapPerPlayer[1] = cTapped;
					sdata->buttonHeldPerPlayer[0] = hHeld;
					sdata->buttonHeldPerPlayer[1] = cHeld;
					sdata->AnyPlayerTap = hTapped | cTapped;
					sdata->AnyPlayerHold = hHeld | cHeld;

					// === Netplay state sync (position/velocity correction) ===
					{
						int localId = Netplay_GetLocalPlayerId();
						int remoteId = (localId == 0) ? 1 : 0;
						u32 frameNum = gGT->frameTimer_VsyncCallback;

						// Send local driver state every 5 frames (or immediately if resync requested)
						static u32 s_lastStateSend = 0;
						if (g_NetplayStateRequested)
						{
							g_NetplayStateRequested = 0;
							s_lastStateSend = 0;
						}
						if (frameNum - s_lastStateSend >= 5 || s_lastStateSend == 0)
						{
							s_lastStateSend = frameNum;
							struct Driver *localDriver = gGT->drivers[localId];
							if (localDriver != NULL)
							{
								struct NetplayStatePayload st;
								st.frameNum = frameNum;
								st.posX = localDriver->posCurr.x;
								st.posY = localDriver->posCurr.y;
								st.posZ = localDriver->posCurr.z;
								st.rotX = localDriver->rotCurr.x;
								st.rotY = localDriver->rotCurr.y;
								st.rotZ = localDriver->rotCurr.z;
								st.speed = localDriver->speed;
								st.kartState = (u8)localDriver->kartState;
								st.lapIndex = localDriver->lapIndex;
								st.heldItemID = (u8)localDriver->heldItemID;
								st.numHeldItems = (u8)localDriver->numHeldItems;
								st.actionsFlagSet = localDriver->actionsFlagSet;
								st.velX = localDriver->velocity.x;
								st.velY = localDriver->velocity.y;
								st.velZ = localDriver->velocity.z;
								Netplay_SendStatePacket(&st);

								// T8: Broadcast race finish when local player finishes
								if ((localDriver->actionsFlagSet & 0x2000000) && !Netplay_IsRemoteFinished(localId))
								{
									Netplay_MarkRemoteFinished(localId);
									Netplay_BroadcastPacket(NETPLAY_PACKET_FINISHED, 1, &localId);
								}
							}
						}

						// Apply received remote state correction
						{
							struct NetplayStatePayload remoteState;
							if (Netplay_DequeueState(remoteId, &remoteState))
							{
								Netplay_ClearState(remoteId);
								struct Driver *remoteDriver = gGT->drivers[remoteId];
								if (remoteDriver != NULL)
								{
									// Calculate distance to target (squared)
									s32 dx = remoteState.posX - remoteDriver->posCurr.x;
									s32 dy = remoteState.posY - remoteDriver->posCurr.y;
									s32 dz = remoteState.posZ - remoteDriver->posCurr.z;
									u32 distSq = (u32)(dx*dx + dy*dy + dz*dz);

									// Large desync: snap instantly
									if (distSq > 0x1000000)
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
										// Small drift: lerp 20% toward target
										remoteDriver->posCurr.x += dx / 5;
										remoteDriver->posCurr.y += dy / 5;
										remoteDriver->posCurr.z += dz / 5;
									}

									// Always apply non-positional state
									remoteDriver->rotCurr.x = remoteState.rotX;
									remoteDriver->rotCurr.y = remoteState.rotY;
									remoteDriver->rotCurr.z = remoteState.rotZ;
									remoteDriver->kartState = (char)remoteState.kartState;
									remoteDriver->lapIndex = remoteState.lapIndex;
									remoteDriver->heldItemID = (char)remoteState.heldItemID;
									remoteDriver->numHeldItems = (char)remoteState.numHeldItems;
									remoteDriver->actionsFlagSet = remoteState.actionsFlagSet;

									// Set remote finished flag if received state says so
									if (remoteState.actionsFlagSet & 0x2000000)
										Netplay_MarkRemoteFinished(remoteId);
								}
							}
						}

						// T7: Apply remote crate hits
						struct NetplayCrateHit crateHit;
						while (Netplay_DequeueCrateHit(&crateHit))
						{
							if (gGT->level1 != NULL && gGT->level1->ptrInstDefs != NULL)
							{
								int num = gGT->level1->numInstances;
								for (int i = 0; i < num; i++)
								{
									struct Instance *inst = gGT->level1->ptrInstDefs[i].ptrInstance;
									if (inst != NULL &&
									    inst->matrix.t[0] == crateHit.posX &&
									    inst->matrix.t[1] == crateHit.posY &&
									    inst->matrix.t[2] == crateHit.posZ)
									{
										inst->scale[0] = 0;
										inst->scale[1] = 0;
										inst->scale[2] = 0;
										// Start cooldown if thread exists
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

						// T10: Send position checksum every 30 frames
						static u32 s_lastChecksumSend = 0;
						if (frameNum - s_lastChecksumSend >= 30)
						{
							s_lastChecksumSend = frameNum;
							struct Driver *localDriver = gGT->drivers[localId];
							if (localDriver != NULL)
							{
								struct NetplayChecksumPayload cp;
								cp.frameNum = frameNum;
							cp.checksum = (u32)(localDriver->posCurr.x * 3 +
							                   localDriver->posCurr.y * 7 +
							                   localDriver->posCurr.z * 11 +
							                   localDriver->speed * 13 +
							                   localDriver->lapIndex * 17 +
							                   localDriver->actionsFlagSet * 19);
							Netplay_BroadcastPacket(NETPLAY_PACKET_CHECKSUM, sizeof(cp), &cp);

							// Compare with last remote checksum; request resync if mismatch
							if (g_NetplayRemoteChecksumFrame != 0 &&
							    g_NetplayRemoteChecksumValue != cp.checksum)
							{
								Netplay_BroadcastPacket(NETPLAY_PACKET_STATE_REQ, 0, NULL);
								g_NetplayRemoteChecksumFrame = 0;
							}
							}
						}
					}

					// T9: Handle remote disconnect during race
					if (g_NetplayDisconnected)
					{
						g_NetplayDisconnected = 0;
						g_NetplayRacing = 0;
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
