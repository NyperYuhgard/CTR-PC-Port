#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 overlay 230 0x800b5c8c-0x800b615c.
void MM_Video_VLC_Decode(void)
{
	s16 oldDecodeState;
	int backloc;
	int result;
	u_long size;
	s16 freeSectors;
	s16 overSectors;
	u_long *sectorData;
	u_long *sectorHeader[2];
	int waitTime;
	CdlLOC *sectorLoc;

	waitTime = 10; // frames

	// free sectors and over sectors
	StRingStatus(&freeSectors, &overSectors);

	backloc = StGetBackloc(&V230.cdLocation2);

	oldDecodeState = V230.field9_0x1a;
	if ((V230.field9_0x1a == 1) && ((V230.RING_SIZE - (V230.RING_SIZE >> 2)) <= freeSectors))
	{
		V230.field14_0x24++;

		if (400 < V230.field14_0x24)
		{
			V230.field14_0x24 = 0;
			StClearRing();
			V230.field8_0x18 = 0;
			V230.frameCount = 0;
			V230.field19_0x30 = 0;
			V230.field20_0x34 = 0;
			V230.field0_0x0 = -1;
			V230.field1_0x4 = -1;
			V230.field2_0x8 = 0;
			V230.field13_0x22 = 0;
			V230.field14_0x24 = 0;
			V230.field21_0x38 = 0xffffffff;
			V230.field9_0x1a = oldDecodeState;

			MM_Video_KickCD(&V230.cdLocation1);
		}

		V230.drawNextFrame = 0;
		return;
	}

	V230.field14_0x24 = 0;

	// Scrapbook
	if (((V230.flags & 8) == 0) && (freeSectors < (V230.RING_SIZE >> 4)))
	{
		MM_Video_KickCD(&V230.cdLocation2);
	}

	if (backloc == V230.field20_0x34)
	{
		V230.field13_0x22++;
		if (0x40 < V230.field13_0x22)
		{
			V230.field13_0x22 = 0;
			V230.drawNextFrame = 0;
			StClearRing();
			V230.field19_0x30 = 0;
			V230.field0_0x0 = -1;
			V230.field1_0x4 = -1;
			V230.field2_0x8 = 0;
			V230.field14_0x24 = 0;
			V230.field20_0x34 = 0;
			V230.field13_0x22 = 0;
			V230.field21_0x38 = 0xffffffff;

			MM_Video_KickCD(&V230.cdLocation3);
		}
	}
	else
	{
		V230.field13_0x22 = 0;
	}

	V230.field9_0x1a = 0;

	// if reached end of video,
	// choose to loop or not loop
	if ((V230.field0_0x0 < 0) &&

	    // length of video
	    ((V230.numFrames <= backloc ||

	      (backloc < V230.field20_0x34))))
	{
		// scrapbook not track select,
		// if video is not looping
		if ((V230.flags & 4) == 0)
		{
			do
			{
				// 9 = CdlPause
				result = CdControl(9, 0, 0);
			} while (result == 0);
			// end of scrapbook
			V230.field8_0x18 = 1;
		}

		// track select, not scrapbook,
		// if video is looping
		else
		{
			V230.field21_0x38 = 0xffffffff;
			if (V230.field1_0x4 < 1)
			{
				MM_Video_KickCD(&V230.cdLocation1);

				if (backloc == V230.numFrames)
				{
					V230.field1_0x4 = CdPosToInt(&V230.cdLocation2);
				}
				else
				{
					V230.field0_0x0 = CdPosToInt(&V230.cdLocation2);
					V230.field0_0x0--;
					V230.field2_0x8 = 0;
				}
			}
			else
			{
				if (backloc != V230.numFrames)
				{
					result = CdPosToInt(&V230.cdLocation2);
					if (V230.field1_0x4 < result)
					{
						V230.field0_0x0 = CdPosToInt(&V230.cdLocation2);
						V230.field0_0x0 = V230.field0_0x0 + -1;
						V230.field2_0x8 = 0;

						MM_Video_KickCD(&V230.cdLocation1);
					}
					V230.field1_0x4 = -1;
				}
			}
		}
	}

	V230.field19_0x30 = V230.frameCount;
	V230.field20_0x34 = backloc;

LAB_800b5fec:

	// retrieve data with timeout (10 frames)
	do
	{
		result = StGetNext(&sectorData, sectorHeader);
		if (result == 0)
		{
			// sector->frameCount
			V230.frameCount = *(int *)((char *)sectorHeader[0] + 8);

			if (V230.frameCount == V230.field19_0x30)
			{
				StFreeRing(sectorData);
				goto LAB_800b5fec;
			}

			if (0 < V230.field0_0x0)
			{
				// sector->loc
				sectorLoc = (CdlLOC *)((char *)sectorHeader[0] + 0x1c);
				result = CdPosToInt(sectorLoc);

				waitTime = 10;

				if (V230.field0_0x0 <= result)
				{
					V230.field2_0x8 = 1;
					StFreeRing(sectorData);
					goto LAB_800b5fec;
				}
				if (V230.field2_0x8 == 1)
				{
					V230.field0_0x0 = -1;
					V230.field2_0x8 = 0;
					V230.field20_0x34 = backloc;
				}
			}

			size = DecDCTBufSize(sectorData);

			if (size <= V230.field25_0x48)
			{
				// sector->loc
				sectorLoc = (CdlLOC *)((char *)sectorHeader[0] + 0x1c);
				V230.cdLocation3 = *sectorLoc;

				// VLC Decode
				// last parameter is "VLC Table"
				DecDCTvlc2(sectorData, V230.in_Buf[V230.field10_0x1c], sdata->ptrVlcTable);

				// ready to draw next frame
				V230.drawNextFrame = 1;

				StFreeRing(sectorData);
				return;
			}
			V230.drawNextFrame = 0;
			StFreeRing(sectorData);
			return;
		}
		waitTime--;
	} while (waitTime != 0);

	V230.drawNextFrame = 0;
}
