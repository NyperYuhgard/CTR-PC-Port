#include <common.h>

void MainInit_JitPoolsNew(struct GameTracker *gGT)
{
	// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003b43c-0x8003b6d0 for the retail path.
	u32 gameMode = gGT->gameMode1;
	int poolScale = 0x800;
	if ((gameMode & ADVENTURE_ARENA) == 0)
	{
		poolScale = 0x1000;
		if ((gameMode & MAIN_MENU) != 0)
			poolScale = 0x400;
	}

	int renderBucketSize = 0x800;
	if ((gameMode & ADVENTURE_ARENA) == 0)
	{
		renderBucketSize = 0x1000;
		if ((gameMode & MAIN_MENU) != 0)
		{
			renderBucketSize = 0x400;
			if (gGT->levelID == ADVENTURE_GARAGE)
				renderBucketSize = 0x800;
		}
	}

	MEMPACK_PushState();

	JitPool_Init(&gGT->JitPools.thread, (renderBucketSize * 3) >> 7, sizeof(struct Thread), rdata.s_ThreadPool);
	JitPool_Init(&gGT->JitPools.instance, renderBucketSize >> 5, sizeof(struct Instance) + (sizeof(struct InstDrawPerPlayer) * gGT->numPlyrCurrGame),
	             rdata.s_InstancePool);
	JitPool_Init(&gGT->JitPools.smallStack, (poolScale * 0x19) >> 10, 0x48, rdata.s_SmallStackPool);
	JitPool_Init(&gGT->JitPools.mediumStack, poolScale >> 7, 0x88, rdata.s_MediumStackPool);

	int numDriver = poolScale >> 9;
	if ((gameMode & MAIN_MENU) != 0)
		numDriver = 4;
	JitPool_Init(&gGT->JitPools.largeStack, numDriver, 0x670, rdata.s_LargeStackPool);

	int numParticle = poolScale >> 5;
	JitPool_Init(&gGT->JitPools.particle, numParticle, sizeof(struct Particle), rdata.s_ParticlePool);
	JitPool_Init(&gGT->JitPools.oscillator, numParticle, 0x18, rdata.s_OscillatorPool);
	JitPool_Init(&gGT->JitPools.rain, poolScale >> 9, sizeof(struct RainLocal), rdata.s_RainPool);

#ifndef CTR_NATIVE
	gGT->ptrRenderBucketInstance = MEMPACK_AllocMem(renderBucketSize);
#else
	// NOTE(aalhendi): Native reuses static RDATA scratch for existing PC memory headroom.
	gGT->ptrRenderBucketInstance = (void *)((uintptr_t)&rdata.s_STATIC_GNORMALZ[0] + 148);
#endif

	for (int i = 0; i < 3; i++)
	{
		struct JitPool *pool = (struct JitPool *)((char *)&gGT->JitPools.smallStack + (sizeof(struct JitPool) * i));
		int *pointer = (int *)pool->free.first;
		while (pointer != (int *)0x0)
		{
			*(int **)(pointer + 2) = pointer + 2;
			pointer = (int *)*pointer;
		}
	}

	for (int i = 0; i < gGT->numPlyrCurrGame; i++)
	{
		data.PtrClipBuffer[i] = MEMPACK_AllocMem(MainDB_GetClipSize(gGT->levelID, gGT->numPlyrCurrGame) << 2);
	}
}
