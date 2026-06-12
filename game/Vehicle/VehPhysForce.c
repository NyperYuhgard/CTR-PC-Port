#include <common.h>


// NOTE(aalhendi): ASM-verified helper for NTSC-U 926 0x8005e104-0x8005e214.
void VehPhysForce_ConvertSpeedToVecOut(struct Driver *driver, Vec3 *vel)
{
	int yAngle = driver->axisRotationY;
	int ySine = MATH_Sin(yAngle);
	int yCos = MATH_Cos(yAngle);
	int yComponent = FP_MULT(driver->speed, yCos);

	int xAngle = driver->axisRotationX;
	int xCos = MATH_Cos(xAngle);
	int xSine = MATH_Sin(xAngle);

	vel->x = FP_MULT(yComponent, xSine);
	vel->y = FP_MULT(driver->speed, ySine);
	vel->z = FP_MULT(yComponent, xCos);
}

void VehPhysForce_ConvertSpeedToVec(struct Driver *driver)
{
	VehPhysForce_ConvertSpeedToVecOut(driver, &driver->velocity);
}

enum
{
	VEH_PHYS_FORCE_ACTION_PREVENT_ACCEL = 0x8,
	VEH_PHYS_FORCE_ACTION_AIRBORNE = 0x80000,
	VEH_PHYS_FORCE_ACTION_800000 = 0x800000,
	VEH_PHYS_FORCE_ACTION_BACK_SKID = 0x800,
	VEH_PHYS_FORCE_ACTION_FRONT_SKID = 0x1000,
	VEH_PHYS_FORCE_QUAD_LOW_GRAVITY = 0x2,
};

static int VehPhysForce_OnGravity_Abs(int value)
{
	return value < 0 ? -value : value;
}

static u32 VehPhysForce_OnGravity_ReadWord(const void *base, int offset)
{
	return *(const u32 *)(const void *)((const char *)base + offset);
}

static s16 VehPhysForce_OnGravity_ReadS16(const void *base, int offset)
{
	return *(const s16 *)(const void *)((const char *)base + offset);
}

static u32 VehPhysForce_OnGravity_PackS16Pair(s32 lo, s32 hi)
{
	return ((u32)(u16)lo) | ((u32)(u16)hi << 16);
}

static void VehPhysForce_OnGravity_SetLightMatrixTranspose(const MATRIX *m)
{
	u32 r0 = VehPhysForce_OnGravity_ReadWord(m, 0x0);
	u32 r1 = VehPhysForce_OnGravity_ReadWord(m, 0x4);
	u32 r2 = VehPhysForce_OnGravity_ReadWord(m, 0x8);
	u32 r3 = VehPhysForce_OnGravity_ReadWord(m, 0xc);
	s16 r4 = VehPhysForce_OnGravity_ReadS16(m, 0x10);
	const u32 highMask = 0xffff0000u;

	// NOTE(aalhendi): Retail constructs a transposed copy of matrixMovingDir in
	// the light matrix regs before projecting velocity and gravity into local space.
	CTC2((r0 & 0xffffu) | (r1 & highMask), 8);
	CTC2((r3 & 0xffffu) | (r0 & highMask), 9);
	CTC2((r2 & 0xffffu) | (r3 & highMask), 10);
	CTC2((r1 & 0xffffu) | (r2 & highMask), 11);
	CTC2((u32)(s32)r4, 12);
}

static Vec3 VehPhysForce_OnGravity_RotateVectorLocal(const MATRIX *m, s16 vx, s16 vy, s16 vz)
{
	Vec3 out;

	(void)m;
	MTC2(VehPhysForce_OnGravity_PackS16Pair(vx, vy), 0);
	MTC2((u32)(u16)vz, 1);
	gte_mvmva(1, 1, 0, 3, 0);
	out.x = MFC2_S(25);
	out.y = MFC2_S(26);
	out.z = MFC2_S(27);

	return out;
}

static Vec3 VehPhysForce_OnGravity_RotateVector(const MATRIX *m, s16 vx, s16 vy, s16 vz)
{
	Vec3 out;

	(void)m;
	MTC2(VehPhysForce_OnGravity_PackS16Pair(vx, vy), 0);
	MTC2((u32)(u16)vz, 1);
	gte_mvmva(1, 0, 0, 3, 0);
	out.x = MFC2_S(25);
	out.y = MFC2_S(26);
	out.z = MFC2_S(27);

	return out;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8005e214-0x8005ea60
void VehPhysForce_OnGravity(struct Driver *driver, Vec3 *velocity)
{
	int elapsedTimeMS = sdata->gGT->elapsedTimeMS;

	gte_SetRotMatrix(&driver->matrixMovingDir);
	VehPhysForce_OnGravity_SetLightMatrixTranspose(&driver->matrixMovingDir);

	Vec3 localVelocity = VehPhysForce_OnGravity_RotateVectorLocal(&driver->matrixMovingDir, (s16)velocity->x, (s16)velocity->y, (s16)velocity->z);
	int originalLocalZ = localVelocity.z;
	int gravityY = -driver->const_Gravity;
	struct QuadBlock *underDriver = driver->underDriver;

	// NOTE(aalhendi): Retail does not branch before reading this flag. Native
	// can reach this frame after TeleportSelf clears underDriver and before
	// fixed collision repopulates it; PS1 low-RAM behavior is no low-gravity
	// flag, while host C would crash on a null dereference.
	if ((underDriver != NULL) && ((underDriver->quadFlags & VEH_PHYS_FORCE_QUAD_LOW_GRAVITY) != 0))
	{
		gravityY = (-driver->const_Gravity * 41) / 100;
	}

	gravityY = (gravityY * elapsedTimeMS) >> 5;
	Vec3 localGravity = VehPhysForce_OnGravity_RotateVectorLocal(&driver->matrixMovingDir, 0, (s16)gravityY, 0);

	if (((localGravity.z < 0) && (driver->unk_offset3B2 > 0)) || ((localGravity.z > 0) && (driver->unk_offset3B2 < 0)))
	{
		localGravity.z = 0;
	}

	u32 actionsFlagSet = driver->actionsFlagSet;
	int speedApprox = driver->speedApprox;
	int baseSpeed = driver->baseSpeed;
	if ((actionsFlagSet & VEH_PHYS_FORCE_ACTION_PREVENT_ACCEL) || ((baseSpeed > 0) && (speedApprox < 0)) ||
	    ((driver->baseSpeed < 0) && (driver->speedApprox > 0)))
	{
		localGravity.x = 0;
		localGravity.z = 0;
	}

	int originalLocalX = localVelocity.x;
	int originalLocalY = localVelocity.y;
	int localX = originalLocalX + localGravity.x;
	int localY = originalLocalY + localGravity.y;
	int localZ = originalLocalZ + localGravity.z;

	int maxForwardSpeed = driver->fireSpeed + driver->unk47E;
	if ((localZ > maxForwardSpeed) && (originalLocalZ < maxForwardSpeed))
	{
		localZ = maxForwardSpeed;
	}

	int minForwardSpeed = driver->fireSpeed - (driver->unk47E >> 1);
	if ((localZ < minForwardSpeed) && (originalLocalZ > minForwardSpeed))
	{
		localZ = minForwardSpeed;
	}

	int maxPerpendicularSpeed = driver->unk480;
	if ((localX > maxPerpendicularSpeed) && (originalLocalX < maxPerpendicularSpeed))
	{
		localX = maxPerpendicularSpeed;
	}

	int minPerpendicularSpeed = -maxPerpendicularSpeed;
	if ((localX < minPerpendicularSpeed) && (originalLocalX > minPerpendicularSpeed))
	{
		localX = minPerpendicularSpeed;
	}

	u32 terrainFlags = driver->terrainMeta1->flags;
	int terminalVelocity = driver->const_TerminalVelocity;
	if ((localY < 0) && ((terrainFlags & 0x80) != 0) && (originalLocalY < -0x100))
	{
		terminalVelocity = 0x100;
		originalLocalY = -0x100;
	}

	int clampedLocalY = localY;
	if (localY > terminalVelocity)
	{
		clampedLocalY = originalLocalY;
		if (originalLocalY < terminalVelocity)
		{
			clampedLocalY = terminalVelocity;
		}
	}

	int minTerminalVelocity = -terminalVelocity;
	if (clampedLocalY < minTerminalVelocity)
	{
		clampedLocalY = originalLocalY;
		if (originalLocalY > minTerminalVelocity)
		{
			clampedLocalY = minTerminalVelocity;
		}
	}

	localY = clampedLocalY;

	if (driver->kartState == KS_MASK_GRABBED)
	{
		localX = 0;
		localZ = 0;
	}
	else if (((driver->actionsFlagSetPrevFrame & ACTION_TOUCH_GROUND) != 0) || (driver->kartState == KS_BLASTED) ||
	         ((driver->unknowndriverBaseSpeed < driver->speedApprox) && (driver->terrainMeta2->unk_0x8 < 0x100)))
	{
		int perpendicularFriction;
		int forwardFriction;

		if ((actionsFlagSet & VEH_PHYS_FORCE_ACTION_PREVENT_ACCEL) == 0)
		{
			if (baseSpeed == 0)
			{
				perpendicularFriction = driver->const_NoPedalFriction_Perpendicular;
				forwardFriction = driver->const_NoPedalFriction_Forward;

				if (driver->driverRankItemValue == 5)
				{
					perpendicularFriction = driver->const_BrakeFriction << 4;
					forwardFriction = perpendicularFriction;
				}
			}
			else
			{
				int absSpeedApprox = VehPhysForce_OnGravity_Abs(speedApprox);
				if ((absSpeedApprox < 0x301) || (((baseSpeed < 1) || (speedApprox >= 0)) && ((baseSpeed >= 0) || (speedApprox < 1))))
				{
					if (driver->kartState == KS_DRIFTING)
					{
						perpendicularFriction = driver->const_DriftCurve;
						forwardFriction = driver->const_DriftFriction;
					}
					else
					{
						perpendicularFriction = driver->const_PedalFriction_Perpendicular;
						forwardFriction = driver->const_PedalFriction_Forward;

						if (absSpeedApprox > 0x300)
						{
							int absBaseSpeed = VehPhysForce_OnGravity_Abs(baseSpeed);
							if (absSpeedApprox < (absBaseSpeed >> 1))
							{
								actionsFlagSet |= VEH_PHYS_FORCE_ACTION_BACK_SKID;
							}
						}
					}
				}
				else
				{
					perpendicularFriction = driver->const_PedalFriction_Perpendicular;
					forwardFriction = driver->const_BrakeFriction;

					if (absSpeedApprox > 0x300)
					{
						actionsFlagSet |= VEH_PHYS_FORCE_ACTION_BACK_SKID;
					}
				}
			}
		}
		else
		{
			int absSpeedApprox = VehPhysForce_OnGravity_Abs(speedApprox);
			if (absSpeedApprox > 0x300)
			{
				actionsFlagSet |= VEH_PHYS_FORCE_ACTION_BACK_SKID;
			}

			perpendicularFriction = driver->const_BrakeFriction;
			if (driver->driverRankItemValue == 5)
			{
				perpendicularFriction <<= 4;
				forwardFriction = perpendicularFriction;
			}
			else if (driver->kartState == KS_BLASTED)
			{
				perpendicularFriction = (perpendicularFriction * 3) >> 2;
				forwardFriction = perpendicularFriction;
			}
			else
			{
				forwardFriction = perpendicularFriction;
				if (driver->kartState == KS_SPINNING)
				{
					perpendicularFriction >>= 1;
					forwardFriction = perpendicularFriction;
				}
			}
		}

		perpendicularFriction = (perpendicularFriction * elapsedTimeMS) >> 5;
		forwardFriction = (forwardFriction * elapsedTimeMS) >> 5;

		int terrainFrictionScale = driver->terrainMeta1->unk_0x20[0];
		if (terrainFrictionScale != 0x100)
		{
			perpendicularFriction = (terrainFrictionScale * perpendicularFriction) >> 8;
			forwardFriction = (terrainFrictionScale * forwardFriction) >> 8;
		}

		int terrainTimer = driver->filler_short;
		if (terrainTimer < 0)
		{
			int absLocalX = localX;
			if (terrainTimer == -0x140)
			{
				absLocalX = VehPhysForce_OnGravity_Abs(localX);
				perpendicularFriction = absLocalX >> 1;
			}
			else
			{
				perpendicularFriction += (perpendicularFriction * driver->const_unk444) >> 8;
				if (perpendicularFriction < 0)
				{
					perpendicularFriction = 0;
				}

				absLocalX = VehPhysForce_OnGravity_Abs(localX);
			}

			if (absLocalX > 0)
			{
				actionsFlagSet |= VEH_PHYS_FORCE_ACTION_BACK_SKID | VEH_PHYS_FORCE_ACTION_FRONT_SKID;
				GAMEPAD_ShockForce1(driver, 4, 0x7f);
				GAMEPAD_ShockFreq(driver, 4, 0);
			}

			terrainTimer += elapsedTimeMS;
			if (terrainTimer > 0)
			{
				terrainTimer = 0;
			}
			driver->filler_short = (s16)terrainTimer;
		}
		else if (terrainTimer > 0)
		{
			terrainTimer -= elapsedTimeMS;
			if (terrainTimer < 0)
			{
				terrainTimer = 0;
			}

			perpendicularFriction += (perpendicularFriction * driver->const_unk444) >> 8;
			driver->filler_short = (s16)terrainTimer;
			if (perpendicularFriction < 0)
			{
				perpendicularFriction = 0;
			}
		}

		if (((actionsFlagSet & VEH_PHYS_FORCE_ACTION_800000) == 0) && ((terrainFlags & 0x80) != 0))
		{
			int absSideSpeed = localX >> 3;
			if (absSideSpeed < 0)
			{
				absSideSpeed = -absSideSpeed;
			}
			if (perpendicularFriction < absSideSpeed)
			{
				perpendicularFriction = absSideSpeed;
			}

			int minForwardFriction = 0;
			if ((localZ == 0) || (baseSpeed == 0) || ((localZ ^ baseSpeed) >= 0))
			{
				if (((baseSpeed <= localZ) || (localZ > 0)) && ((localZ <= baseSpeed) || (localZ < 0)))
				{
					goto APPLY_TERRAIN_FRICTION;
				}

				minForwardFriction = VehPhysForce_OnGravity_Abs(localZ - baseSpeed) >> 1;
			}
			else
			{
				minForwardFriction = VehPhysForce_OnGravity_Abs(localZ >> 1);
			}

			if (forwardFriction < minForwardFriction)
			{
				forwardFriction = minForwardFriction;
			}
		}

	APPLY_TERRAIN_FRICTION:
	{
		int xFriction = perpendicularFriction;
		if ((terrainFlags & 0x100) != 0)
		{
			xFriction = (perpendicularFriction * 3) >> 2;
			if (xFriction < forwardFriction)
			{
				xFriction = forwardFriction;
			}
		}

		localX = VehCalc_InterpBySpeed(localX, xFriction, 0);
		localZ = VehCalc_InterpBySpeed(localZ, forwardFriction, 0);
	}
	}

	*velocity = VehPhysForce_OnGravity_RotateVector(&driver->matrixMovingDir, (s16)localX, (s16)localY, (s16)localZ);
	driver->actionsFlagSet = actionsFlagSet;

	if ((actionsFlagSet & VEH_PHYS_FORCE_ACTION_AIRBORNE) == 0)
	{
		if ((driver->boolFirstFrameSinceRevEngine != 0) && (localZ != 0))
		{
			driver->forwardDir = (localZ < 0) ? -1 : 1;
			driver->boolFirstFrameSinceRevEngine = 0;
			goto CHECK_ROLLBACK_FROM_PREVIOUS_DIRECTION;
		}

		if (originalLocalZ < 0)
		{
			goto SET_FORWARD_DIRECTION_IF_NONNEGATIVE;
		}

		if (localZ < 0)
		{
			driver->forwardDir = -1;
		}

		if (originalLocalZ > 0)
		{
			goto CHECK_ROLLBACK_FROM_FORWARD;
		}

	SET_FORWARD_DIRECTION_IF_NONNEGATIVE:
		if (localZ >= 0)
		{
			driver->forwardDir = 1;
		}
	}

CHECK_ROLLBACK_FROM_PREVIOUS_DIRECTION:
	if (originalLocalZ >= 0)
	{
		goto CHECK_ROLLBACK_FROM_FORWARD;
	}

	if (localZ <= 0)
	{
		return;
	}
	goto START_ROLLBACK;

CHECK_ROLLBACK_FROM_FORWARD:
	if (localZ < 0)
	{
		goto START_ROLLBACK;
	}
	if (originalLocalZ > 0)
	{
		return;
	}
	if (localZ <= 0)
	{
		return;
	}

START_ROLLBACK:
	if (driver->StartRollback_0x280 != 0)
	{
		driver->unknownTraction++;
	}
	driver->StartRollback_0x280 = 0x280;
}

static u32 VehPhysForce_OnApplyForces_PackS16Pair(s32 lo, s32 hi)
{
	return ((u32)(u16)lo) | ((u32)(u16)hi << 16);
}

static Vec3 VehPhysForce_OnApplyForces_RotateVector(const MATRIX *m, s16 vx, s16 vy, s16 vz)
{
	Vec3 out;

	// NOTE(aalhendi): Retail loads matrixFacingDir into CP2 color matrix regs
	// and runs opcode 0x4c6012 for this center-offset calculation.
	gte_SetColorMatrix(m);
	MTC2(VehPhysForce_OnApplyForces_PackS16Pair(vx, vy), 0);
	MTC2((u32)(u16)vz, 1);
	gte_mvmva(1, 2, 0, 3, 0);
	out.x = MFC2_S(25);
	out.y = MFC2_S(26);
	out.z = MFC2_S(27);

	return out;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8005ea60-0x8005ebac.
void VehPhysForce_OnApplyForces(struct Thread *thread, struct Driver *driver)
{
	(void)thread;

	const int maxMudSinkYLevel = FP(-1);
	const int maxSpeed = FP8(100);
	driver->speed = min(driver->speed, maxSpeed);

	/* origin of driver model is center-bottom of kart,
	use orientation matrix, and half-radius {0, 25, 0},
	to find the "true" center of the 3D model */
	driver->originToCenter = VehPhysForce_OnApplyForces_RotateVector(&driver->matrixFacingDir, 0, 25, 0);

	VehPhysForce_ConvertSpeedToVecOut(driver, &driver->velocity);

	if ((driver->underDriver) && (driver->underDriver->terrain_type == TERRAIN_MUD))
	{
		if (driver->posCurr.y > maxMudSinkYLevel)
		{
			// sink slower as you approach the mud's bottom
			int sinkSpeed = maxMudSinkYLevel - driver->posCurr.y;
			driver->velocity.y = max(driver->velocity.y, sinkSpeed);
		}
	}

	VehPhysForce_OnGravity(driver, &driver->velocity);

	const SVec3 up = {.x = FP(0), .y = FP(1), .z = FP(0)};
	driver->normalVecUP = up;
	driver->AxisAngle1_normalVec = up;
	driver->unkAA = 0; // driver quadblock flags?
	driver->currBlockTouching = nullptr;

	driver->velocity.x += driver->accel.x;
	driver->velocity.z += driver->accel.z;
	driver->velocity.y += driver->accel.y;
}

// NOTE(aalhendi): Keep bucket typed; native O2 can drop punned stack writes.
// First 0x18 bytes match `struct BucketSearchParams`; the trailing hitDir
// fields are written by `VehPhysCrash_AnyTwoCars`.
struct VehPhysForce_CollideDrivers_Search
{
	struct BucketSearchParams bucket;
	s16 hitDir[4];
};

_Static_assert(offsetof(struct VehPhysForce_CollideDrivers_Search, bucket) == 0);
_Static_assert(offsetof(struct VehPhysForce_CollideDrivers_Search, hitDir) == sizeof(struct BucketSearchParams));

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8005ebac-0x8005ee34.
void VehPhysForce_CollideDrivers(struct Thread *thread, struct Driver *driver)
{
	u32 stepFlagSet = driver->stepFlagSet;

	driver->velocity.x -= driver->accel.x;
	driver->velocity.y -= driver->accel.y;
	driver->velocity.z -= driver->accel.z;

	if ((stepFlagSet & 0x4000) != 0)
	{
		driver->unkAA |= 1;
	}

	if ((stepFlagSet & 2) != 0)
	{
		VehFire_Increment(driver, 0x78, TURBO_PAD | FREEZE_RESERVES_ON_TURBO_PAD, 0x800);
	}
	else if ((stepFlagSet & 1) != 0)
	{
		if ((sdata->gGT->gameMode2 & CHEAT_TURBOPAD) == 0)
		{
			VehFire_Increment(driver, 0x3c0, TURBO_PAD | FREEZE_RESERVES_ON_TURBO_PAD, 0x100);
		}
		else
		{
			VehFire_Increment(driver, 0x78, TURBO_PAD | FREEZE_RESERVES_ON_TURBO_PAD, 0x800);
		}
	}

	if ((stepFlagSet & 0x8000) != 0)
	{
		thread->inst->vertSplit = 0;
		thread->inst->flags |= 0x2000;
	}
	else
	{
		thread->inst->flags &= ~0x2000;
	}

	if ((thread->flags & 0x1000) == 0)
	{
		struct VehPhysForce_CollideDrivers_Search search;

		search.bucket.pos[0] = (s16)(driver->posCurr.x >> 8);
		search.bucket.pos[1] = (s16)(driver->posCurr.y >> 8);
		search.bucket.pos[2] = (s16)(driver->posCurr.z >> 8);
		search.bucket.th = NULL;
		search.bucket.radius = 0x7fffffff;

		PROC_CollidePointWithBucket(thread->siblingThread, &search.bucket);
		PROC_CollidePointWithBucket(sdata->gGT->threadBuckets[ROBOT].thread, &search.bucket);

		if (search.bucket.th != NULL)
		{
			int radiusSum = thread->driver_HitRadius + search.bucket.th->driver_HitRadius;

			if (search.bucket.radius < radiusSum * radiusSum)
			{
				VehPhysCrash_AnyTwoCars(thread, (u16 *)&search, &driver->velocity);
			}
		}
	}

	if ((driver->unkAA & 2) != 0)
	{
		int diffX = (driver->posCurr.x >> 8) - driver->spsHitPos[0];
		int diffZ = (driver->posCurr.z >> 8) - driver->spsHitPos[2];
		int floorDiffY = ((driver->quadBlockHeight >> 8) - driver->spsHitPos[1]) + 4;

		if ((driver->spsNormalVec[0] * diffX + driver->spsNormalVec[1] * floorDiffY + driver->spsNormalVec[2] * diffZ) < 0)
		{
			int diffY = (driver->posCurr.y >> 8) - driver->spsHitPos[1];

			driver->velocity.x += diffX << 6;
			driver->velocity.y += diffY << 6;
			driver->velocity.z += diffZ << 6;
		}
	}
}

static int VehPhysForce_TranslateMatrix_Abs(int value)
{
	return value < 0 ? -value : value;
}

static int VehPhysForce_TranslateMatrix_Div256TowardZero(int value)
{
	if (value < 0)
	{
		value += 0xff;
	}

	return value >> 8;
}

static u32 VehPhysForce_TranslateMatrix_PackS16Pair(s32 lo, s32 hi)
{
	return ((u32)(u16)lo) | ((u32)(u16)hi << 16);
}

static Vec3 VehPhysForce_TranslateMatrix_RotateVector(const MATRIX *m, s16 vx, s16 vy, s16 vz)
{
	Vec3 out;

	// NOTE(aalhendi): Retail loads matrixFacingDir into CP2 control regs and
	// rotates the baked animation offset with opcode 0x486012. The GTE side
	// effects are observable by later vehicle wobble paths.
	gte_SetRotMatrix(m);
	MTC2(VehPhysForce_TranslateMatrix_PackS16Pair(vx, vy), 0);
	MTC2((u32)(u16)vz, 1);
	gte_mvmva(1, 0, 0, 3, 0);
	out.x = MFC2_S(25);
	out.y = MFC2_S(26);
	out.z = MFC2_S(27);

	return out;
}

static struct MatrixND *VehPhysForce_TranslateMatrix_GetBakedEntry(u8 matrixArray, u8 matrixIndex)
{
	return &((struct MatrixND *)data.bakedGteMath[matrixArray].physEntry)[matrixIndex];
}

static u8 VehPhysForce_TranslateMatrix_RemapIndex(u8 matrixIndex, int fromArray, int toArray)
{
	int fromLast = data.bakedGteMath[fromArray].numEntries - 1;
	int blend = 0x100 - (((u32)matrixIndex << 8) / fromLast);

	if (blend < 0)
	{
		blend = 0;
	}
	if (blend > 0x100)
	{
		blend = 0x100;
	}

	return (u8)((blend * (data.bakedGteMath[toArray].numEntries - 1)) >> 8);
}

static void VehPhysForce_TranslateMatrix_ResetMatrixAnim(struct Driver *d)
{
	d->matrixArray = 0;
	d->matrixIndex = 0;
}

static void VehPhysForce_TranslateMatrix_UpdateSquashStretch(struct Instance *inst, struct Driver *d)
{
	if (d->kartState == KS_WARP_PAD)
	{
		return;
	}

	if ((d->kartState == KS_MASK_GRABBED) && ((d->actionsFlagSet & ACTION_TOUCH_GROUND) == 0))
	{
		int scaleXZ;

		inst->scale[1] = d->jumpSquishStretch + 0xccc;

		scaleXZ = 0xccc - VehPhysForce_TranslateMatrix_Div256TowardZero(d->jumpSquishStretch * 0x28);
		if (scaleXZ < 0x400)
		{
			scaleXZ = 0x400;
		}

		inst->scale[0] = scaleXZ;
		inst->scale[2] = scaleXZ;
		return;
	}

	int jumpHeightCurr = d->jumpHeightCurr;
	int targetSquish = -800;

	if ((d->actionsFlagSet & 0x400) == 0)
	{
		int smoothed = (d->jumpSquishStretch2 * 9 + jumpHeightCurr * 7) >> 4;
		int delta = (d->jumpSquishStretch2 - smoothed) * 4;

		if (VehPhysForce_TranslateMatrix_Abs(delta) < 0x960)
		{
			delta = 0;
		}

		if (((d->actionsFlagSet | d->actionsFlagSetPrevFrame) & 2) == 0)
		{
			if (delta < -800)
			{
				delta = -800;
			}
		}
		else if (delta < -0x640)
		{
			delta = -0x640;
		}

		if (delta > 800)
		{
			delta = 800;
		}

		targetSquish = delta;
	}

	if ((d->hazardTimer > 0) && ((d->hazardTimer & 0x80) == 0) && (targetSquish > -800))
	{
		targetSquish = -800;
	}

	if (((d->actionsFlagSet & ACTION_TOUCH_GROUND) == 0) && (jumpHeightCurr < 0))
	{
		int mapped = VehCalc_MapToRange(-jumpHeightCurr, 0, 0xa00, 0x280, 0x320);

		if (targetSquish < mapped)
		{
			targetSquish = mapped;
		}

		d->jumpSquishStretch2 = jumpHeightCurr;
	}

	if ((d->instTntRecv != NULL) && (d->instTntRecv->scale[1] < 2500))
	{
		targetSquish += (d->instTntRecv->scale[1] - 0x800) * 2;
	}

	if (VehPhysForce_TranslateMatrix_Abs(d->jumpSquishStretch) < VehPhysForce_TranslateMatrix_Abs(targetSquish))
	{
		d->jumpSquishStretch = targetSquish;
	}

	d->jumpSquishStretch = VehCalc_InterpBySpeed(d->jumpSquishStretch, 300, 0);
	d->jumpSquishStretch2 = (d->jumpSquishStretch2 * 9 + jumpHeightCurr * 7) >> 4;

	if (d->squishTimer != 0)
	{
		inst->scale[1] = 0;
	}
	else if (inst->scale[1] == 0)
	{
		if (d->instSelf->thread->modelIndex == 0x18)
		{
			OtherFX_Play_Echo(0x5b, 1, (d->actionsFlagSet >> 16) & 1);
		}

		inst->scale[1] = d->jumpSquishStretch + 0xccc;
		d->matrixArray = 5;
		d->matrixIndex = 0;
	}
	else
	{
		inst->scale[1] = VehCalc_InterpBySpeed(inst->scale[1], 0xa0, d->jumpSquishStretch + 0xccc);
	}

	int scaleXZ = 0xccc - VehPhysForce_TranslateMatrix_Div256TowardZero(d->jumpSquishStretch * 0xa0);
	inst->scale[0] = VehCalc_InterpBySpeed(inst->scale[0], 0xa0, scaleXZ);
	inst->scale[2] = VehCalc_InterpBySpeed(inst->scale[2], 0xa0, scaleXZ);
}

static void VehPhysForce_TranslateMatrix_UpdateMatrixAnimation(struct Driver *d)
{
	if ((d->reserves == 0) || (d->fireSpeed < d->const_Speed_ClassStat) || ((d->actionsFlagSet & 0x80) != 0))
	{
		if (d->matrixArray == 2)
		{
			d->matrixArray = 3;
			d->matrixIndex = 0;
		}
		else if (d->matrixArray == 1)
		{
			d->matrixIndex = VehPhysForce_TranslateMatrix_RemapIndex(d->matrixIndex, 1, 3);
			d->matrixArray = 3;
		}
		else if (d->matrixArray == 3)
		{
			d->matrixIndex++;
			if (d->matrixIndex >= data.bakedGteMath[3].numEntries)
			{
				VehPhysForce_TranslateMatrix_ResetMatrixAnim(d);
			}
		}
	}
	else
	{
		if (d->matrixArray == 1)
		{
			d->matrixIndex++;
			if (d->matrixIndex >= data.bakedGteMath[1].numEntries)
			{
				d->matrixArray = 2;
				d->matrixIndex = 0;
			}
		}
		else if (d->matrixArray == 3)
		{
			d->matrixIndex = VehPhysForce_TranslateMatrix_RemapIndex(d->matrixIndex, 3, 1);
			d->matrixArray = 1;
		}
		else if (d->matrixArray == 0)
		{
			d->matrixArray = 1;
			d->matrixIndex = 0;
		}
	}

	if (d->matrixArray == 5)
	{
		d->matrixIndex++;
		if (d->matrixIndex >= data.bakedGteMath[5].numEntries)
		{
			VehPhysForce_TranslateMatrix_ResetMatrixAnim(d);
		}
	}
}

static void VehPhysForce_TranslateMatrix_CopyFacingMatrix(struct Instance *inst, struct Driver *d)
{
	inst->matrix.m[0][0] = d->matrixFacingDir.m[0][0];
	inst->matrix.m[0][1] = d->matrixFacingDir.m[0][1];
	inst->matrix.m[0][2] = d->matrixFacingDir.m[0][2];
	inst->matrix.m[1][0] = d->matrixFacingDir.m[1][0];
	inst->matrix.m[1][1] = d->matrixFacingDir.m[1][1];
	inst->matrix.m[1][2] = d->matrixFacingDir.m[1][2];
	inst->matrix.m[2][0] = d->matrixFacingDir.m[2][0];
	inst->matrix.m[2][1] = d->matrixFacingDir.m[2][1];
	inst->matrix.m[2][2] = d->matrixFacingDir.m[2][2];
}

static void VehPhysForce_TranslateMatrix_UpdateInstanceMatrix(struct Instance *inst, struct Driver *d)
{
	int screenOffsetY = ((s8)d->Screen_OffsetY * 3) >> 3;

	if (d->matrixArray == 0)
	{
		VehPhysForce_TranslateMatrix_CopyFacingMatrix(inst, d);

		inst->matrix.t[0] = d->posCurr.x >> 8;
		inst->matrix.t[1] = (d->posCurr.y >> 8) + screenOffsetY;
		inst->matrix.t[2] = d->posCurr.z >> 8;
	}
	else
	{
		struct MatrixND *entry = VehPhysForce_TranslateMatrix_GetBakedEntry(d->matrixArray, d->matrixIndex);
		s16 *entryVec = (s16 *)entry;
		Vec3 rotated;

		MatrixRotate(&inst->matrix, &d->matrixFacingDir, (MATRIX *)(void *)((u8 *)entry + 8));

		rotated = VehPhysForce_TranslateMatrix_RotateVector(&d->matrixFacingDir, entryVec[0], entryVec[1], entryVec[2]);

		inst->matrix.t[0] = (d->posCurr.x + rotated.x) >> 8;
		inst->matrix.t[1] = ((d->posCurr.y + rotated.y) >> 8) + screenOffsetY;
		inst->matrix.t[2] = (d->posCurr.z + rotated.z) >> 8;
	}

	if (d->squishTimer != 0)
	{
		inst->matrix.t[0] += (d->AxisAngle2_normalVec[0] * 0x13) >> 12;
		inst->matrix.t[1] += (d->AxisAngle2_normalVec[1] * 0x13) >> 12;
		inst->matrix.t[2] += (d->AxisAngle2_normalVec[2] * 0x13) >> 12;
	}
}

static void VehPhysForce_TranslateMatrix_HideWake(struct Instance *inst, struct Driver *d)
{
	struct Instance *wake = d->wakeInst;

	if (inst->matrix.t[1] > 0)
	{
		inst->flags &= ~SPLIT_LINE;
	}

	if (wake != NULL)
	{
		wake->flags |= HIDE_MODEL;
		d->wakeScale = 0;
		wake->scale[0] = 0;
		wake->scale[2] = d->wakeScale;
	}
}

static void VehPhysForce_TranslateMatrix_SpawnWakeParticle(struct Driver *d)
{
	struct Particle *p = Particle_Init(0, sdata->gGT->iconGroup[9], &data.emSet_Falling[0]);

	if (p != NULL)
	{
		p->unk18 = d->instSelf->unk50;
		p->driverInst = d->instSelf;
		p->unk19 = d->driverID;
	}
}

static void VehPhysForce_TranslateMatrix_SetWakeRotation(struct Instance *wake, struct Driver *d)
{
	s32 sin = MATH_Sin(d->angle);
	s32 cos = MATH_Cos(d->angle);

	// Retail writes paired matrix shorts with word stores.
	*(s32 *)(void *)&wake->matrix.m[0][0] = cos;
	*(s32 *)(void *)&wake->matrix.m[0][2] = sin;
	*(s32 *)(void *)&wake->matrix.m[1][1] = 0x1000;
	*(s32 *)(void *)&wake->matrix.m[2][0] = -sin;
	wake->matrix.m[2][2] = (s16)cos;
}

static void VehPhysForce_TranslateMatrix_UpdateWake(struct Instance *inst, struct Driver *d)
{
	struct Instance *wake;

	if (inst->matrix.t[1] >= 0)
	{
		VehPhysForce_TranslateMatrix_HideWake(inst, d);
		return;
	}

	if ((inst->matrix.t[1] < -0x4f) || ((inst->flags & SPLIT_LINE) == 0))
	{
		VehPhysForce_TranslateMatrix_HideWake(inst, d);
		return;
	}

	wake = d->wakeInst;
	if (wake == NULL)
	{
		return;
	}

	wake->flags &= ~HIDE_MODEL;
	wake->unk50 = inst->unk50 + 1;
	wake->unk51 = inst->unk51 - 1;

	wake->matrix.t[0] = inst->matrix.t[0];
	wake->matrix.t[1] = 0;
	wake->matrix.t[2] = inst->matrix.t[2];

	VehPhysForce_TranslateMatrix_SetWakeRotation(wake, d);

	if (d->wakeScale == 0)
	{
		d->wakeScale = 0x1000;

		if (sdata->gGT->numPlyrCurrGame < 2)
		{
			if ((VehPhysForce_TranslateMatrix_Abs(d->speed) > 0xc00) && (d->posPrev.y > -0x200))
			{
				int i;

				for (i = 10; i > 0; i--)
				{
					VehPhysForce_TranslateMatrix_SpawnWakeParticle(d);
				}
			}
		}
	}
	else if (sdata->gGT->numPlyrCurrGame < 2)
	{
		if (VehPhysForce_TranslateMatrix_Abs(d->speed) > 0xc00)
		{
			VehPhysForce_TranslateMatrix_SpawnWakeParticle(d);
		}
	}

	wake->scale[0] = d->wakeScale;
	wake->scale[2] = d->wakeScale;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8005ee34-0x8005f89c
void VehPhysForce_TranslateMatrix(struct Thread *thread, struct Driver *driver)
{
	struct Instance *inst = thread->inst;

	VehPhysForce_TranslateMatrix_UpdateSquashStretch(inst, driver);
	VehPhysForce_RotAxisAngle(&driver->matrixFacingDir, driver->AxisAngle2_normalVec, driver->rotCurr.y);
	VehPhysForce_TranslateMatrix_UpdateMatrixAnimation(driver);
	VehPhysForce_TranslateMatrix_UpdateInstanceMatrix(inst, driver);
	VehPhysForce_TranslateMatrix_UpdateWake(inst, driver);
}

struct VehPhysForceTrigPair
{
	s32 sin;
	s32 cos;
};

static u32 VehPhysForce_ReadWord(const void *base, int offset)
{
	return *(const u32 *)(const void *)((const char *)base + offset);
}

static int VehPhysForce_CountLeadingSignBits(s32 value)
{
	u32 bits = (u32)value;
	u32 sign = bits >> 31;
	int count = 0;

	while (count < 32 && (((bits >> (31 - count)) & 1) == sign))
		count++;

	return count;
}

static struct VehPhysForceTrigPair VehPhysForce_TrigAngleSinCos(int angle)
{
	u32 packed = VehPhysForce_ReadWord(&data.trigApprox[angle & 0x3ff], 0);
	struct VehPhysForceTrigPair pair;

	if ((angle & 0x400) == 0)
	{
		pair.sin = (s16)packed;
		pair.cos = (s16)(packed >> 16);

		if ((angle & 0x800) != 0)
		{
			pair.sin = -pair.sin;
			pair.cos = -pair.cos;
		}
	}
	else
	{
		pair.sin = (s16)(packed >> 16);
		pair.cos = (s16)packed;

		if ((angle & 0x800) != 0)
			pair.sin = -pair.sin;
		else
			pair.cos = -pair.cos;
	}

	return pair;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8005f89c-0x8005fb4c.
void VehPhysForce_RotAxisAngle(MATRIX *m, s16 *normVec, s16 angle)
{
	s32 normalX = normVec[0];
	s32 normalY = normVec[1];
	s32 normalZ = normVec[2];
	struct VehPhysForceTrigPair trig = VehPhysForce_TrigAngleSinCos(angle);
	s32 normalXSq = normalX * normalX;
	s32 normalZSq = normalZ * normalZ;
	s32 crossXZ = normalX * -normalZ;
	s32 denom = normalXSq + normalZSq;
	s32 scaledSinY = (trig.sin * normalY) >> 12;
	s32 scaledCosY = (trig.cos * normalY) >> 12;
	s32 outX = (s16)scaledSinY;
	s32 outY;
	s32 outZ = (s16)scaledCosY;

	m->m[0][1] = (s16)normalX;
	m->m[1][1] = (s16)normalY;
	m->m[2][1] = (s16)normalZ;

	if (denom == 0)
	{
		s32 dot = trig.sin * normalX + trig.cos * normalZ;

		if (normalY < 0)
			outX = -outX;

		outY = -dot >> 12;
	}
	else
	{
		int shift = 0x14 - VehPhysForce_CountLeadingSignBits(denom);
		s32 sinRemainder;
		s32 cosRemainder;
		s32 divX;
		s32 divZ;
		s32 dot;

		if (shift > 0)
		{
			normalXSq >>= shift;
			normalZSq >>= shift;
			crossXZ >>= shift;
			denom >>= shift;
		}

		sinRemainder = trig.sin - scaledSinY;
		cosRemainder = trig.cos - scaledCosY;
		divX = (sinRemainder * normalZSq + cosRemainder * crossXZ) / denom;
		divZ = (sinRemainder * crossXZ + cosRemainder * normalXSq) / denom;
		dot = trig.sin * normalX + trig.cos * normalZ;

		outX = (s16)(outX + (s16)divX);
		outY = -dot >> 12;
		outZ = (s16)(outZ + (s16)divZ);
	}

	m->m[0][2] = (s16)outX;
	m->m[1][2] = (s16)outY;
	m->m[2][2] = (s16)outZ;

	// NOTE(aalhendi): Retail uses GTE `OP(sf=12)` with the normal vector and the generated
	// third column to derive the first matrix column. The partial register
	// writes are intentional; callers observe this exact GTE state.
	CTC2((u32)normalX, 0);
	CTC2((u32)normalY, 2);
	CTC2((u32)normalZ, 4);
	MTC2((u32)(s32)m->m[0][2], 9);
	MTC2((u32)(s32)m->m[1][2], 10);
	MTC2((u32)(s32)m->m[2][2], 11);
	gte_op12();
	m->m[0][0] = (s16)MFC2_S(25);
	m->m[1][0] = (s16)MFC2_S(26);
	m->m[2][0] = (s16)MFC2_S(27);
}

static u32 VehPhysForce_CounterSteer_PackS16Pair(s32 lo, s32 hi)
{
	return ((u32)(u16)lo) | ((u32)(u16)hi << 16);
}

static SVec3 VehPhysForce_CounterSteer_RotateVector(const MATRIX *m, s16 vx, s16 vy, s16 vz)
{
	SVec3 out;
	(void)m;

	// NOTE(aalhendi): PhysTerrainSlope already loaded matrixMovingDir into GTE
	// rotation regs, matching retail before VehPhysForce_CounterSteer runs.
	MTC2(VehPhysForce_CounterSteer_PackS16Pair(vx, vy), 0);
	MTC2((u32)(u16)vz, 1);
	gte_mvmva(1, 0, 0, 3, 0);
	out.x = (s16)MFC2(25);
	out.y = (s16)MFC2(26);
	out.z = (s16)MFC2(27);

	return out;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8005fb4c-0x8005fc8c.
void VehPhysForce_CounterSteer(struct Driver *driver)
{
	driver->accel.x = 0;
	driver->accel.y = 0;
	driver->accel.z = 0;

	int speedApprox = abs(driver->speedApprox);
	if (speedApprox <= FP8(3) || driver->actionsFlagSet & ACTION_WARP || driver->kartState == KS_CRASHING || driver->set_0xF0_OnWallRub ||
	    !(driver->actionsFlagSet & ACTION_TOUCH_GROUND) || driver->terrainMeta1->counterSteerRatio == 0)
	{
		return;
	}

	int angleLimit = (u8)driver->angleMaxCounterSteer;
	int angle = driver->turnAngleCurr - driver->turnAnglePrev;
	if (angle > angleLimit)
	{
		angle = angleLimit;
	}
	else if (angle < -angleLimit)
	{
		angle = -angleLimit;
	}

	int sine = MATH_Sin(angle);
	int counterSteerStrength = (driver->terrainMeta1->counterSteerRatio * -8000) >> 8;
	SVec3 accel = VehPhysForce_CounterSteer_RotateVector(&driver->matrixMovingDir, (s16)((counterSteerStrength * sine) >> 12), 0, 0);

	driver->accel = accel;
}
