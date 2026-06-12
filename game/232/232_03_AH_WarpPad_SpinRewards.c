#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800abdfc-0x800abf48.
void AH_WarpPad_SpinRewards(struct Instance *prizeInst, struct WarpPad *warppadObj, int index, int x, int y, int z)
{
	s16 *specLight;
	u32 modelID;
	u32 trig;
	u32 thirds;

	ConvertRotToMatrix(&prizeInst->matrix, &warppadObj->spinRot_Prize[0]);

	modelID = prizeInst->model->id;

	if (modelID != STATIC_TROPHY) // if not trophy (no specLight on trophy)
	{
		if (modelID == STATIC_GEM) // gem
			specLight = &warppadObj->specLightGem[0];
		else
		{
			if (modelID == STATIC_RELIC) // relic
				specLight = &warppadObj->specLightRelic[0];
			else
			{
				if (modelID == STATIC_TOKEN) // token
					specLight = &warppadObj->specLightToken[0];
				else
					goto SpinReward;
			}
		}
		Vector_SpecLightSpin3D(prizeInst, &warppadObj->spinRot_Prize[0], specLight);
	}

SpinReward:

	// initialized as 0x555*index, but not const
	thirds = warppadObj->thirds[index];

	trig = MATH_Sin(thirds);
	prizeInst->matrix.t[1] = y + ((trig << 6) >> 0xc) + 0x100;

	// do not use original "thirds",
	// set new value without "+="
	thirds = 0x555 * index + warppadObj->spinRot_Rewards[1];

	trig = MATH_Sin(thirds);
	prizeInst->matrix.t[0] = x + (trig * 0xA0 >> 0xc);

	trig = MATH_Cos(thirds);
	prizeInst->matrix.t[2] = z + (trig * 0xA0 >> 0xc);
}
