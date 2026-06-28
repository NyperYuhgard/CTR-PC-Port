#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80031c78-0x80031d30
void *LOAD_ReadDirectory(char *filename)
{
        CdlFILE cdlFile;
        char buf[8];

        BFDBG_PRINTF("ReadDirectory: START — filename=\"%s\"", filename ? filename : "(null)");

        CDSYS_SetMode_StreamData();

#if defined(CTR_NATIVE)
        {
                int bfMode = NativeBigfile_GetMode();

                BFDBG_PRINTF("ReadDirectory: bigfile mode = %d (%s)",
                        bfMode,
                        bfMode == 0 ? "PACKED" :
                        bfMode == 1 ? "UNPACKED" :
                        bfMode == 2 ? "HYBRID" : "UNKNOWN");

                /* UNPACKED mode: build the directory from BIGFILE.TXT.
                   No BIGFILE.BIG is needed at all. */
                if (bfMode == NATIVE_BIGFILE_MODE_UNPACKED)
                {
                        struct BigHeader *bh;

                        BFDBG_PRINTF("ReadDirectory: UNPACKED path — building directory from BIGFILE.TXT");

                        bh = MEMPACK_AllocMem(0x4000 /*, filename*/);
                        if (bh == NULL)
                        {
                                BFDBG_PRINTF_ERR("ReadDirectory: MEMPACK_AllocMem(0x4000) returned NULL!");
                                return NULL;
                        }

                        BFDBG_PRINTF("ReadDirectory: buffer allocated at %p, calling BuildDirectory...", bh);

                        if (NativeBigfile_BuildDirectory(bh, 0x4000) < 0)
                        {
                                BFDBG_PRINTF_ERR("ReadDirectory: BuildDirectory FAILED — freeing buffer");
                                MEMPACK_ReallocMem(0);
                                return NULL;
                        }

                        BFDBG_PRINTF("ReadDirectory: BuildDirectory OK — numEntry=%d, reallocating...", bh->numEntry);

                        MEMPACK_ReallocMem(
                                sizeof(struct BigHeader) +
                                sizeof(struct BigEntry) * bh->numEntry);

                        sdata->ptrBigfileCdPos_2 = bh;

                        BFDBG_PRINTF("ReadDirectory: UNPACKED done — ptrBigfileCdPos_2 = %p, %d entries",
                                (void *)bh, bh->numEntry);
                        return bh;
                }

                /* HYBRID mode: read the directory from BIGFILE.BIG (same as
                   packed mode), but also load the BIGFILE.TXT path map so
                   that LOAD_ReadFile_ex can check for folder overrides. */
                if (bfMode == NATIVE_BIGFILE_MODE_HYBRID)
                {
                        BFDBG_PRINTF("ReadDirectory: HYBRID path — loading BIGFILE.TXT override map...");
                        /* Load the path map for override lookups.  If this
                           fails we simply continue without overrides. */
                        int mapResult = NativeBigfile_LoadPathMap();
                        BFDBG_PRINTF("ReadDirectory: LoadPathMap result = %d", mapResult);
                }

                /* PACKED mode falls through to the standard CdRead path below */
                if (bfMode == NATIVE_BIGFILE_MODE_PACKED)
                {
                        BFDBG_PRINTF("ReadDirectory: PACKED path — using CdSearchFile/CdRead");
                }
        }
#endif

        BFDBG_PRINTF("ReadDirectory: calling CdSearchFile for \"%s\"...", filename ? filename : "(null)");

        if (CdSearchFile(&cdlFile, filename) == NULL)
        {
                BFDBG_PRINTF_ERR("ReadDirectory: CdSearchFile FAILED for \"%s\" — file not found on disc",
                        filename ? filename : "(null)");
                return NULL;
        }

        BFDBG_PRINTF("ReadDirectory: CdSearchFile OK, reading 8 sectors...");

        struct BigHeader *bh = MEMPACK_AllocMem(0x4000 /*, filename*/);

        // Search for file on disc
        // Set Cd laser to file position
        // Read the bigfile header
        // Wait for read to end
        CdControl(CdlSetloc, &cdlFile, buf);
        if (CdRead(8, (u32 *)bh, 0x80) == 0)
        {
                BFDBG_PRINTF_ERR("ReadDirectory: CdRead returned 0 — read failed");
                return NULL;
        }

        if (CdReadSync(0, 0) != 0)
        {
                BFDBG_PRINTF_ERR("ReadDirectory: CdReadSync failed");
                return NULL;
        }

        // Save position
        bh->cdpos = CdPosToInt(&cdlFile.pos);

        BFDBG_PRINTF("ReadDirectory: CdRead OK — cdpos=%d, numEntry=%d",
                bh->cdpos, bh->numEntry);

        // undo allocation of 0x4000, only use "needed" size
        MEMPACK_ReallocMem(sizeof(struct BigHeader) + sizeof(struct BigEntry) * bh->numEntry);

        sdata->ptrBigfileCdPos_2 = bh;

        BFDBG_PRINTF("ReadDirectory: PACKED/HYBRID done — ptrBigfileCdPos_2 = %p, %d entries",
                (void *)bh, bh->numEntry);
        return bh;
}
