#include <common.h>

static inline u8 *RB_Banner_FirstVertex(struct ModelHeader *mh)
{
	return (u8 *)mh->ptrFrameData + mh->ptrFrameData->vertexOffset;
}

static inline void RB_Banner_SaveVertex(u8 stackIndex, const u8 *vertex)
{
	u32 *slot = CTR_SCRATCHPAD_PTR(u32, stackIndex * 8);

	slot[0] = (u32)vertex[0] | ((u32)vertex[2] << 16);
	slot[1] = (slot[1] & 0xffff0000U) | (u32)vertex[1];
}

static inline int RB_Banner_LoadSavedX(u32 command, u8 stackIndex)
{
	u32 saved;
	u32 *slot;

	saved = *CTR_SCRATCHPAD_PTR(u32, (command >> 13) & 0x7f8);
	slot = CTR_SCRATCHPAD_PTR(u32, stackIndex * 8);
	*CTR_SCRATCHPAD_PTR(u32, 0x300) = saved;
	*CTR_SCRATCHPAD_PTR(u32, 0x304) = slot[1];

	return (s32)(saved << 16) >> 18;
}

static inline u32 RB_Banner_RewriteCommandX(u32 command, int xQuarter, int reusedVertex)
{
	u32 mask = reusedVertex ? 0xf7ff01ffU : 0xffff01ffU;

	return (command & mask) | ((u32)xQuarter << 9);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b53e0-0x800b56c4.
int RB_Banner_Animate_Init(struct ModelHeader *mh)
{
	u32 *cmd;
	u8 *vertex;
	int count = 0;

	if ((s16)(*(u16 *)(void *)mh->ptrCommandList) < 0x40)
		return 0;

	vertex = RB_Banner_FirstVertex(mh);
	cmd = (u32 *)((u8 *)mh->ptrCommandList + 4);

	while (*cmd != 0xffffffffU)
	{
		u32 command = *cmd;

		if ((command & 0xffff0000U) == 0)
		{
			cmd++;
			continue;
		}

		if ((s32)command >= 0)
		{
			u8 stackIndex = ((u8 *)cmd)[2];
			int xQuarter;

			if ((command & 0x04000000U) == 0)
			{
				RB_Banner_SaveVertex(stackIndex, vertex);
				xQuarter = vertex[0] >> 2;
				vertex += 3;
				count++;
				*cmd = RB_Banner_RewriteCommandX(command, xQuarter, 0);
			}
			else
			{
				xQuarter = RB_Banner_LoadSavedX(command, stackIndex);
				*cmd = RB_Banner_RewriteCommandX(command, xQuarter, 1);
			}

			cmd++;
			continue;
		}

		for (int i = 0; i < 3; i++, cmd++)
		{
			command = *cmd;
			u8 stackIndex = ((u8 *)cmd)[2];
			int xQuarter;

			if ((command & 0x04000000U) == 0)
			{
				RB_Banner_SaveVertex(stackIndex, vertex);
				xQuarter = vertex[0] >> 2;
				vertex += 3;
				count++;
				*cmd = RB_Banner_RewriteCommandX(command, xQuarter, 0);
			}
			else
			{
				xQuarter = RB_Banner_LoadSavedX(command, stackIndex);
				*cmd = RB_Banner_RewriteCommandX(command, xQuarter, 1);
			}
		}
	}

	if (sdata->gGT->numPlyrCurrGame >= 4)
	{
		vertex = RB_Banner_FirstVertex(mh);
		for (int i = 0; i < count; i++, vertex += 3)
			vertex[1] = 0x80;
	}

	return count;
}
