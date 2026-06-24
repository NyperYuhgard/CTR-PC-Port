#include <common.h>

static int VehPhysCrash_BounceSelf_Div6Shift9(int value)
{
	s64 product = (s64)value * 0x2aaaaaab;
	int high = (int)(product >> 32);

	return (high >> 9) - (value >> 31);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8005cf64-0x8005d0d0.
int VehPhysCrash_BounceSelf(s16 *normal, Vec3 *origin, Vec3 *vel, int boolOtherDriver)
{
	int diffX = vel->x - origin->x;
	int diffY = vel->y - origin->y;
	int diffZ = vel->z - origin->z;
	int dot = (diffX * normal[0] + diffY * normal[1] + diffZ * normal[2]) >> 0xc;

	if (boolOtherDriver == 0)
	{
		if (dot >= 0)
		{
			return 0;
		}
	}
	else if (dot <= 0)
	{
		return 0;
	}

	int absDot = dot;
	if (absDot < 0)
	{
		absDot = -absDot;
	}

	if (sdata->unk_8008d9f4[0] < absDot)
	{
		sdata->unk_8008d9f4[0] = absDot;
	}

	diffX -= VehPhysCrash_BounceSelf_Div6Shift9(dot * normal[0]);
	diffY -= VehPhysCrash_BounceSelf_Div6Shift9(dot * normal[1]);
	diffZ -= VehPhysCrash_BounceSelf_Div6Shift9(dot * normal[2]);

	vel->x = diffX + origin->x;

	int oldY = vel->y;
	int newY = diffY + origin->y;
	if ((oldY < newY) && (newY > 0x3200))
	{
		newY = 0x3200;
	}
	vel->y = newY;
	vel->z = diffZ + origin->z;

	return 0;
}
