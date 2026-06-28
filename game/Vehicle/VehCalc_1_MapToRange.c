#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80058f9c-0x8005900c.
int VehCalc_MapToRange(int val, int oldMin, int oldMax, int newMin, int newMax)
{
	if (val <= oldMin)
		return newMin;

	if (val >= oldMax)
		return newMax;

	return (newMin + (
	                     // distFromBottom * newRange
	                     (val - oldMin) * (newMax - newMin))

	                     // divide by oldRange
	                     / (oldMax - oldMin)

	);
}
