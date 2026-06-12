struct ZoomData
{
	// get distance by mapping camera speeed from:
	// [speedMin, speedMax] to [distMin, distMax]

	// 0x0
	s16 distMin;
	s16 distMax;

	// 0x4
	s16 speedMin;
	s16 speedMax;

	// 0x8
	u8 percentage1;
	u8 percentage2;

	// 0xA
	s16 angle[3];

	// 0x10
	s16 vertDistance;
};

struct FlyInData
{
	int ptrEnd;
	int ptrStart;
	s16 frameCount1;
	s16 frameCount2;
};

struct CameraDC
{
	// 0x0
	int cameraID;

	// 0x4
	// action,
	// 0x20000 constantly swaps L2 zoom
	u32 action;

	// 0x08
	// camera mode, zoom out and such
	u16 mode;

	// 0x0A
	u16 nearOrFar;

	// 0xC
	u32 unk0xC;

	// 0x10
	// desired rotation
	s16 desiredRot[4];

	// 0x18
	s16 unk18;
	// 0x1a
	s16 unk1A;

	// 0x1c - ptrQuadBlock
	// similar to driver +a0,
	// quadblock camera is currently above
	struct QuadBlock *ptrQuadBlock;

	// 0x20
	// VisMem->0x40[player], quadblock->0x44->0x0
	int *visLeafSrc;

	// 0x24
	// VisMem->0x50[player], quadblock->0x44->0x4
	int *visFaceSrc;

	// 0x28
	// quadblock->0x44->0x8
	// which instances are visible from quadblock
	struct Instance **visInstSrc;

	// 0x2c
	// VisMem->0x60[player]
	int *visOVertSrc;

	// 0x30
	// VisMem->0x70[player],
	int *visSCVertSrc;

	// 0x34
	char unk30fill[0xC];

	// 96b20+14c0

	// 0x40
	int cameraMoveSpeed;

	// 0x44
	struct Driver *driverToFollow;

	// 0x48
	struct PushBuffer *pushBuffer;

	// 0x4C
	// vel[3]
	int unkTriplet1[3];

	// 0x58
	// pos[3]
	int unkTriplet2[3];

	// 0x64
	// rot[3]
	int unkTriplet3[3];

	// 0x70 - flags
	// & 1 - search "+ 0x1508) | 1;", resets RainBuffer cameraPos
	// & 4 - battle end-of-race
	// & 8 - just changed direction (forward or backward) this frame
	// & 0x10 - mask grab
	// & 0x20 - arcade end-of-race (requested)
	// & 0x100 - changes TrackPath from point->0x4 to point->0x9
	// & 0x200 - (aku hints + save/load) transitioning away from player
	// & 0x400 - (aku hints + save/load) snap to player, or transition to player if 0x600
	// & 0x800 - (aku hints + save/load) stationary away from player
	// & 0x1000 - arcade end-of-race (active)
	// & 0x8000 - frozen camera (disables thread, for character select)
	// & 0x10000 - reverse camera
	u32 flags;

	// 0x74 (cam->0x9a is 8 or 0xe)
	s16 driverOffset_CamEyePos[3];
	s16 unk7A;

	// 0x7c (cam->0x9a is 8 or 0xe)
	s16 driverOffset_CamLookAtPos[3];
	s16 unk82;

	// 0x84
	u32 driver5B0_prevFrame;

	// 0x88 - used in CAM_FollowDriver_TrackPath
	void *unk88;

	// 0x8C - Interpolate from fly-in
	// camera to driver, 0x0000 is fly-in,
	// 0x1000 is driver, and between is interpolation
	s16 unk8C;

	// 0x8E - timer for fly-in camera
	// animation at beginning of 1P Arcade,
	// search "+ 0x1526" for more details
	s16 unk8E;

	// 0x90 - used in Spin360
	s16 unk90;

	// zoom variable
	s16 unk92;

	// 0x94
	int unk94;

	// 0x98
	s16 unk98;

	// 0x9a - semi-unused camera mode swap
	s16 cameraMode; // Curr

	// 0x9C
	s16 cameraModePrev; // previous frame

	// 0x9e - frame counter for transition
	s16 frameCounterTransition;

	// 0xa0
	void *currEOR;

	// difference between 8e and 9e?

	// 0xa4
	// union shared between camera modes
	// Spin360 uses 0xa4 for spin speed
	struct
	{
		s16 pos[3];
		s16 rot[3];
	} transitionTo;

	// 0xb0 - next byte
	// union between multiple camera modes
	// TrackPath uses 0xb0 for movement speed
	char unk_b0[8];

	// X axis (0xb8)
	char unk_b8[8];

	// 0xb8 - ms countdown timer
	// 0xbc - ms countdown timer

	// Y axis (0xc0)
	int unk_c0;

	// 0xc0 - distance per frame

	// 0xc2 - frameCountdown

	// 0xc4
	s16 framesZoomingOut;

// Sep3
#if BUILD < UsaRetail

	// 0xc6
	s16 paddingC6;

#else // >= UsaRetail

	// Store data on first frame of BLASTED,
	// Use data on first frame of NOT BLASTED,
	// 8-frame lerp to bring camera back to kart
	struct
	{
		// 0xC6
		s16 boolLerpPending;

		// 0xc8
		s16 unkOffset[2];

		// 0xcc
		s16 desiredRot[4];

		// 0xd4
		s16 desiredPos[3];

		// 0xda
		s16 framesRemaining;

	} BlastedLerp;

// 0xdc - end of struct
#endif

	// 0xC8 bytes large in sep3
	// 0xDC bytes large in usaRetail
};

_Static_assert(sizeof(struct ZoomData) == 0x12);
#if BUILD >= UsaRetail
_Static_assert(sizeof(struct CameraDC) == 0xDC);
#else
_Static_assert(sizeof(struct CameraDC) == 0xC8);
#endif
