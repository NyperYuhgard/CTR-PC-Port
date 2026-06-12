#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800ad2c8-0x800ad3ec.
void AH_WarpPad_ThDestroy(struct Thread *t)
{
	int i;
	struct Instance **instArr;
	struct WarpPad *warppadObj;

	warppadObj = t->object;

	// array of instances in warppad object
	instArr = &warppadObj->inst[0];

	if (instArr[WPIS_CLOSED_1S] != 0)
	{
		INSTANCE_Death(instArr[WPIS_CLOSED_1S]);
		instArr[WPIS_CLOSED_1S] = 0;
	}

	if (instArr[WPIS_CLOSED_10S] != 0)
	{
		INSTANCE_Death(instArr[WPIS_CLOSED_10S]);
		instArr[WPIS_CLOSED_10S] = 0;
	}

	if (instArr[WPIS_CLOSED_X] != 0)
	{
		INSTANCE_Death(instArr[WPIS_CLOSED_X]);
		instArr[WPIS_CLOSED_X] = 0;
	}

	if (instArr[WPIS_CLOSED_ITEM] != 0)
	{
		INSTANCE_Death(instArr[WPIS_CLOSED_ITEM]);
		instArr[WPIS_CLOSED_ITEM] = 0;
	}

	if (instArr[WPIS_OPEN_BEAM] != 0)
	{
		INSTANCE_Death(instArr[WPIS_OPEN_BEAM]);
		instArr[WPIS_OPEN_BEAM] = 0;
	}

	for (i = WPIS_OPEN_RING1; i < WPIS_OPEN_PRIZE1; i++)
	{
		if (instArr[i] != 0)
		{
			INSTANCE_Death(instArr[i]);
			instArr[i] = 0;
		}
	}

	for (i = WPIS_OPEN_PRIZE1; i < WPIS_NUM_INSTANCES; i++)
	{
		if (instArr[i] != 0)
		{
			INSTANCE_Death(instArr[i]);
			instArr[i] = 0;
		}
	}
}
