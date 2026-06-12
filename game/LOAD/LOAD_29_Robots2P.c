#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80032700-0x800327dc.
void LOAD_Robots2P(struct BigHeader *bigfile, int p1, int p2, void (*callback)(struct LoadQueueSlot *))
{
	int i;
	char *robotSet;
	int boolFoundRepeat = 0;

	// 8 sets, but only check 7 cause
	// the last is Gem Cups pack (4 bosses)
	for (i = 0; i < 7; i++)
	{
		robotSet = &data.characterIDs_2P_AIs[4 * i];

		boolFoundRepeat = 0;
		for (int j = 0; j < 4; j++)
		{
			if ((robotSet[j] == p1) || (robotSet[j] == p2))
			{
				boolFoundRepeat = 1;
				break;
			}
		}

		if (!boolFoundRepeat)
			break;
	}

	if (i > 6)
	{
		return;
	}

	data.characterIDs[2] = robotSet[0];
	data.characterIDs[3] = robotSet[1];
	data.characterIDs[4] = robotSet[2];
	data.characterIDs[5] = robotSet[3];

	LOAD_AppendQueue(bigfile, LT_GETADDR, BI_2PARCADEPACK + i, NULL, callback);
}
