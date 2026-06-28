#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80058a60-0x80058ba4.
void VehBirth_SetConsts(struct Driver *driver)
{
	u32 metaPhysSize;
	u32 i;
	struct MetaPhys *metaPhys;
	u8 *d;

	d = (u8 *)driver;

        int engineID = data.MetaDataCharacters[data.characterIDs[driver->driverID]].engineID;

        for (i = 0; i < 65; i++)
        {
                metaPhys = &data.metaPhys[i];

                metaPhysSize = metaPhys->size;

                short srcVal;
                void *src;
                if (engineID >= 0 && engineID < 4)
                {
                        src = &metaPhys->value[engineID];
                }
                else
                {
                        /* ENGINE_UNLIMITED: pick the best value from all columns */
                        srcVal = metaPhys->value[0];
                        {
                                int j;
                                for (j = 1; j < 4; j++)
                                        if (metaPhys->value[j] > srcVal)
                                                srcVal = metaPhys->value[j];
                        }
                        src = &srcVal;
                }
                void *dst = &d[metaPhys->offset];

		if (metaPhysSize == 1)
		{
			*(char *)dst = *(char *)src;
			continue;
		}

		if (metaPhysSize == 2)
		{
			*(s16 *)dst = *(s16 *)src;
			continue;
		}

		*(int *)dst = *(int *)src;
	}

	return;
}
