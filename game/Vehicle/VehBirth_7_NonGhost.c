#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80058d2c-0x80058ec0.
void VehBirth_NonGhost(struct Thread *t, int index)
{
	// model index = DYNAMIC_PLAYER,
	// AI will override this right after
	// the end of the function
	t->modelIndex = DYNAMIC_PLAYER;

	t->driver_HitRadius = 0x40;
	t->driver_unk1 = 0x1000;
	t->driver_unk3E = 0x40;
	t->driver_unk2 = 0;
	t->driver_unk3 = 0;

	struct Driver *d = t->object;
	struct GameTracker *gGT = sdata->gGT;

#ifdef CTR_NATIVE
	fprintf(stdout, "[Netplay] VehBirth_NonGhost: index=%d, gameMode=0x%x, numPlyr=%d\n",
		index, gGT->gameMode1, gGT->numPlyrCurrGame); fflush(stdout);
#endif

	int id = data.characterIDs[0];
	if ((gGT->gameMode1 & 0x2000) == 0)
	{
		id = data.characterIDs[index];
	}
#ifdef CTR_NATIVE
	fprintf(stdout, "[Netplay] VehBirth_NonGhost: index=%d -> characterID=%d ('%s')\n",
		index, id, data.MetaDataCharacters[id].name_Debug); fflush(stdout);
#endif

	struct Model *m = VehBirth_GetModelByName(data.MetaDataCharacters[id].name_Debug);

#ifdef CTR_NATIVE
	fprintf(stdout, "[Netplay] VehBirth_NonGhost: index=%d model=%p\n",
		index, (void*)m); fflush(stdout);
#endif

	struct Instance *inst = INSTANCE_Birth3D(m, m->name, t);

	t->inst = inst;

#ifdef CTR_NATIVE
	fprintf(stdout, "[Netplay] VehBirth_NonGhost: index=%d inst=%p, inst->model=%p\n",
		index, (void*)inst, (void*)(inst ? inst->model : NULL)); fflush(stdout);
#endif

	// Wake
	m = gGT->modelPtr[STATIC_WAKE];
	if (m != 0)
	{
		inst = INSTANCE_Birth3D(m, m->name, 0);
		d->wakeInst = inst;

		if (inst != 0)
		{
			// invisible, anim #1
			inst->flags |= 0x90;
		}

		// sep 3
		// else
		// player %d wake create failed
	}

	/*
	sep 3
	else
	printf("wake not in level\n");
	*/

	inst = t->inst;
	if (index < gGT->numPlyrCurrGame)
		inst->flags |= 0x4000000;

	d->driverID = index;
	d->instSelf = inst;

	VehBirth_TireSprites(t);
#ifdef CTR_NATIVE
	// NOTE(aalhendi): Retail leaves terrainMeta2 unset until COLL_FIXED;
	// native cannot dereference the PS1 low-memory null-space before then.
	d->terrainMeta2 = d->terrainMeta1;
#endif
	VehBirth_SetConsts(d);

	// if you are in cutscene or in main menu
	if ((gGT->gameMode1 & 0x20002000) != 0)
	{
#ifdef CTR_NATIVE
		fprintf(stdout, "[Netplay] VehBirth_NonGhost: index=%d HIDE_MODEL set (gameMode=0x%x)\n",
			index, gGT->gameMode1); fflush(stdout);
#endif
		// dont update, make invisible
		t->funcThTick = VehBirth_NullThread;
		inst->flags |= 0x80;
	}

#ifdef CTR_NATIVE
	fprintf(stdout, "[Netplay] VehBirth_NonGhost: index=%d final inst->flags=0x%x\n",
		index, inst->flags); fflush(stdout);
#endif
}
