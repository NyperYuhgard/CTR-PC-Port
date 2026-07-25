#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80034aa4-0x80034b48 for the retail body.
// NOTE(aalhendi): CTR_NATIVE keeps native input/audio/VRAM bridge work.
void MainDrawCb_Vsync()
{
	struct GameTracker *gGT;

	gGT = sdata->gGT;
	gGT->frameTimer_VsyncCallback++;
	if ((gGT->gameMode1 & PAUSE_ALL) == 0)
		gGT->frameTimer_Confetti++;

	sdata->vsyncTillFlip--;
	gGT->vSync_between_drawSync++;

	// NOTE(aalhend): accumulate root-counter 1 units between VSync callbacks; Timer_GetTime_Total converts them through the retail 0x147e divisor.
	sdata->rcntTotalUnits += GetRCnt(0xf2000001);
	ResetRCnt(0xf2000001);

#ifdef CTR_NATIVE
	// NOTE(aalhendi): Retail calls HOWL unconditionally. Native skips only while
	// channel lists are mid-edit.
	if (sdata->criticalSectionCount == 0)
#endif
	{
		howl_PlayAudio_Update();
	}

#ifdef CTR_NATIVE
	// NOTE(aalhendi): Native owns host input and writes PSX-shaped pad
	// snapshots before retail GAMEPAD_PollVsync consumes them.
	Platform_PollInput();
	// Refresh gamepad-to-slot links so menus/races see current assignments
	GAMEPAD_GetNumConnected(sdata->gGamepads);

	// Zero out gamepad buffers for disabled (NONE) players to prevent stale input
	// s_disabledSlots is in native_input.c, check via Platform_InputGetKeyboardSlot logic
	// Instead, just ensure any unplugged gamepads have zeroed buffers
	for (int i = 0; i < 4; i++)
	{
		struct ControllerPacket *pkt = sdata->gGamepads->gamepad[i].ptrControllerPacket;
		if (pkt == NULL || pkt->plugged != PLUGGED)
		{
			sdata->gGamepads->gamepad[i].buttonsHeldCurrFrame = 0;
			sdata->gGamepads->gamepad[i].buttonsHeldPrevFrame = 0;
			sdata->gGamepads->gamepad[i].buttonsTapped = 0;
			sdata->gGamepads->gamepad[i].buttonsReleased = 0;
			sdata->gGamepads->gamepad[i].stickLX = 0x80;
			sdata->gGamepads->gamepad[i].stickLY = 0x80;
			sdata->gGamepads->gamepad[i].stickRX = 0x80;
			sdata->gGamepads->gamepad[i].stickRY = 0x80;
		}
	}
#endif

	GAMEPAD_PollVsync(sdata->gGamepads);

	return;
}
