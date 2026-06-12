#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80058948-0x80058a60.
struct Model *VehBirth_GetModelByName(char *searchName)
{
	struct Model *m;
	struct Model **models;
	int i;

	// array to character models loaded,
	// maximum of 4, used in VS mode
	models = (struct Model **)&data.driverModelExtras[0];

#define NUM_CHECK 3 // OG game: 3 drivers in VS mode

	for (i = 0; i < NUM_CHECK; i++)
	{
		m = models[i];

		// 12/16 bytes is enough
		if ((m != NULL) && (*(u32 *)&m->name[0] == *(u32 *)&searchName[0]) && (*(u32 *)&m->name[4] == *(u32 *)&searchName[4]) &&
		    (*(u32 *)&m->name[8] == *(u32 *)&searchName[8]) && (*(u32 *)&m->name[12] == *(u32 *)&searchName[12]))
		{
			// character found, return pointer
			return m;
		}
	}

	models = (struct Model **)sdata->PLYROBJECTLIST;

	if (
	    // list is valid, and first element is valid
	    (models != NULL) && (models[0] != NULL))
	{
		// loop until all strings are checked (until current is not nullptr)
		for (i = 0, m = models[i]; m != NULL; i++, m = models[i])
		{
			// 12/16 bytes is enough
			if ((*(u32 *)&m->name[0] == *(u32 *)&searchName[0]) && (*(u32 *)&m->name[4] == *(u32 *)&searchName[4]) &&
			    (*(u32 *)&m->name[8] == *(u32 *)&searchName[8]) && (*(u32 *)&m->name[12] == *(u32 *)&searchName[12]))
			{
				// character found, return pointer
				return m;
			}
		}
	}
	return NULL;
}
