#include <common.h>

/* Debug macros for the bigfile loading path (defined in native_bigfile.h) */

void *LOAD_ReadFile_ex(struct BigHeader *bigfile, u32 loadType, int subfileIndex, void *ptrDst, int *sizePtr, void (*callback)(struct LoadQueueSlot *))
{
        int uVar5;
        CdlLOC cdLoc;
        u8 paramOutput[8];
        void *originalDst;
        int sectorSize;
        int sectorCount;
        int readComplete;

        // NOTE(aalhendi): ASM-verified NTSC-U 926 PS1 path 0x800321b4-0x80032344.
        (void)loadType;
        CDSYS_SetMode_StreamData();

        BFDBG_PRINTF("ReadFile: START — subfile=%d, dst=%p, callback=%s",
                subfileIndex, ptrDst, callback ? "yes" : "no");

#if defined(CTR_NATIVE)
        // NOTE(aalhendi): CTR_NATIVE preserves existing queues that pass 0 for the
        // default bigfile; retail callers are expected to pass the real pointer.
        if (bigfile == NULL)
        {
                bigfile = sdata->ptrBigfile1;
                BFDBG_PRINTF("ReadFile: bigfile was NULL, using ptrBigfile1=%p", (void *)bigfile);
        }
#endif

        if (bigfile == NULL)
        {
                BFDBG_PRINTF_ERR("ReadFile: bigfile is STILL NULL after fallback — cannot read subfile %d", subfileIndex);
                return NULL;
        }

        // get size and offset of subfile
        struct BigEntry *entry = BIG_GETENTRY(bigfile);
        int eSize = entry[subfileIndex].size;
        int eOffs = entry[subfileIndex].offset;

        *sizePtr = eSize;

        BFDBG_PRINTF("ReadFile: subfile=%d, offset=0x%X, size=0x%X (%d bytes)",
                subfileIndex, eOffs, eSize, eSize);

#if defined(CTR_NATIVE)
        /* ---- Mod file override path ----
         *
         * Before checking the BIGFILE/ folder, check if any enabled mod
         * provides a replacement for this subfile via its "files/"
         * directory.  This allows mods to replace ANY asset from the
         * BIGFILE archive by placing it at:
         *
         *   mods/<mod_name>/files/<relative_path_from_BIGFILE.TXT>
         *
         * For example, to replace a character model:
         *   mods/my_mod/files/models/racers/hi/crash.ctr
         *
         * This works in ALL bigfile modes (PACKED, UNPACKED, HYBRID).
         */
        {
                const char *relPath = NativeBigfile_GetRelPath(subfileIndex);
                if (relPath != NULL)
                {
                        FILE *modFile = NativeMods_OpenFile(relPath, "rb");
                        if (modFile != NULL)
                        {
                                long fileSize;
                                int sectorCount;
                                int readOk;

                                BFDBG_PRINTF("ReadFile: subfile=%d, MOD OVERRIDE FOUND — %s",
                                        subfileIndex, relPath);

                                /* Determine file size. */
                                fseek(modFile, 0, SEEK_END);
                                fileSize = ftell(modFile);
                                fseek(modFile, 0, SEEK_SET);

                                if (fileSize > 0)
                                {
                                        *sizePtr = (int)fileSize;
                                        sectorCount = (int)((fileSize + 0x7ffU) >> 0xb);
                                        originalDst = ptrDst;

                                        /* Allocate buffer if none provided. */
                                        if (ptrDst == NULL)
                                        {
                                                int sectorSize = sectorCount << 0xb;
                                                ptrDst = (void *)MEMPACK_AllocMem(sectorSize);
                                                if (ptrDst == NULL)
                                                {
                                                        BFDBG_PRINTF_ERR("ReadFile: subfile=%d, mod override MEMPACK_AllocMem(%d) FAILED",
                                                                subfileIndex, sectorSize);
                                                        fclose(modFile);
                                                        return NULL;
                                                }
                                                BFDBG_PRINTF("ReadFile: subfile=%d, mod override allocated %d bytes at %p",
                                                        subfileIndex, sectorSize, ptrDst);
                                                {
                                                        struct LoadQueueSlot *lqs = &data.currSlot;
                                                        lqs->flags |= LT_MEMPACK;
                                                        lqs->ptrDestination = ptrDst;
                                                        lqs->size_UNUSED = *sizePtr;
                                                }
                                        }
                                        else
                                        {
                                                struct LoadQueueSlot *lqs = &data.currSlot;
                                                lqs->flags &= ~LT_MEMPACK;
                                                lqs->ptrDestination = ptrDst;
                                                lqs->size_UNUSED = *sizePtr;
                                        }

                                        /* Read file contents. */
                                        {
                                                size_t bytesRead = fread(ptrDst, 1, (size_t)fileSize, modFile);
                                                readOk = ((long)bytesRead == fileSize) ? 1 : 0;
                                        }
                                        fclose(modFile);

                                        if (readOk)
                                        {
                                                BFDBG_PRINTF("ReadFile: subfile=%d, MOD OVERRIDE READ OK — %ld bytes",
                                                        subfileIndex, fileSize);

                                                /* Invoke callback synchronously if requested. */
                                                if (callback != NULL)
                                                {
                                                        sdata->callbackCdReadSuccess = callback;
                                                        data.currSlot.ptrBigfileCdPos_UNUSED = bigfile;
                                                        data.currSlot.subfileIndex = subfileIndex;
                                                        callback(&data.currSlot);
                                                }

                                                /* Trim allocation to actual size. */
                                                if ((callback == NULL) && (originalDst == NULL))
                                                {
                                                        MEMPACK_ReallocMem(*sizePtr);
                                                }

                                                return ptrDst;
                                        }

                                        /* Read failed — fall through to standard paths. */
                                        BFDBG_PRINTF_ERR("ReadFile: subfile=%d, MOD OVERRIDE READ FAILED — falling through",
                                                subfileIndex);
                                        *sizePtr = eSize;
                                        if (originalDst == NULL && ptrDst != NULL)
                                        {
                                                MEMPACK_ReallocMem(0);
                                                ptrDst = NULL;
                                        }
                                }
                                else
                                {
                                        fclose(modFile);
                                        BFDBG_PRINTF_ERR("ReadFile: subfile=%d, mod override exists but size <= 0 (%ld)",
                                                subfileIndex, fileSize);
                                }
                        }
                }
        }

        /* ---- Unpacked / Hybrid override path ----
         *
         * If the bigfile system is in UNPACKED or HYBRID mode and the
         * requested subfile has a corresponding file in the BIGFILE/
         * folder, read it directly from disk instead of going through
         * the CdControl/CdRead sector-based path.
         *
         * This is the core mechanism that makes modding easy: just
         * extract the files you want to change into BIGFILE/ and the
         * game will pick them up automatically.
         */
        {
                int bfMode = NativeBigfile_GetMode();

                if (bfMode != NATIVE_BIGFILE_MODE_PACKED)
                {
                        BFDBG_PRINTF("ReadFile: subfile=%d, checking unpacked override (mode=%s)...",
                                subfileIndex,
                                bfMode == 1 ? "UNPACKED" : "HYBRID");

                        if (NativeBigfile_HasUnpackedFile(subfileIndex))
                        {
                                int unpackedSize = NativeBigfile_GetSubfileSize(subfileIndex);

                                BFDBG_PRINTF("ReadFile: subfile=%d, UNPACKED OVERRIDE found (size=%d)",
                                        subfileIndex, unpackedSize);

                                if (unpackedSize > 0)
                                {
                                        originalDst = ptrDst;

                                        /* Use the unpacked file's actual size.
                                           Modded files may be larger or smaller
                                           than the original. */
                                        *sizePtr = unpackedSize;
                                        sectorCount = (unpackedSize + 0x7ffU) >> 0xb;

                                        /* If no address given, allocate room. */
                                        if (ptrDst == NULL)
                                        {
                                                sectorSize = sectorCount << 0xb;
                                                ptrDst = (void *)MEMPACK_AllocMem(sectorSize);
                                                if (ptrDst == NULL)
                                                {
                                                        BFDBG_PRINTF_ERR("ReadFile: subfile=%d, MEMPACK_AllocMem(%d) FAILED",
                                                                subfileIndex, sectorSize);
                                                        return NULL;
                                                }

                                                BFDBG_PRINTF("ReadFile: subfile=%d, allocated %d bytes at %p",
                                                        subfileIndex, sectorSize, ptrDst);

                                                {
                                                        struct LoadQueueSlot *lqs = &data.currSlot;
                                                        lqs->flags |= LT_MEMPACK;
                                                        lqs->ptrDestination = ptrDst;
                                                        lqs->size_UNUSED = unpackedSize;
                                                }
                                        }
                                        else
                                        {
                                                struct LoadQueueSlot *lqs = &data.currSlot;
                                                lqs->flags &= ~LT_MEMPACK;
                                                lqs->ptrDestination = ptrDst;
                                                lqs->size_UNUSED = unpackedSize;
                                        }

                                        /* Read the file directly. */
                                        BFDBG_PRINTF("ReadFile: subfile=%d, calling ReadSubfile (maxBytes=0x%X)...",
                                                subfileIndex, sectorCount << 0xb);

                                        int bytesRead = NativeBigfile_ReadSubfile(
                                                subfileIndex, ptrDst, sectorCount << 0xb);

                                        if (bytesRead >= 0)
                                        {
                                                BFDBG_PRINTF("ReadFile: subfile=%d, UNPACKED READ OK — %d bytes",
                                                        subfileIndex, bytesRead);

                                                /* If a callback was requested, invoke it
                                                   synchronously (native CdRead is already
                                                   synchronous, so this matches existing
                                                   behavior). */
                                                if (callback != NULL)
                                                {
                                                        sdata->callbackCdReadSuccess = callback;

                                                        /* Set up the currSlot so the callback
                                                           can access the result.

                                                           IMPORTANT: Do NOT overwrite
                                                           callbackFuncPtr here!  It was
                                                           set by LOAD_AppendQueue to the
                                                           end-user callback (e.g.
                                                           LOAD_Callback_DriverModels) and
                                                           LOAD_DramFileCallback reads it
                                                           to decide whether to chain.
                                                           Overwriting it with the
                                                           intermediate callback breaks
                                                           the chain and freezes the
                                                           loading state machine. */
                                                        data.currSlot.ptrBigfileCdPos_UNUSED = bigfile;
                                                        data.currSlot.subfileIndex = subfileIndex;
                                                        /* data.currSlot.callbackFuncPtr = callback; -- REMOVED: was destroying the user callback */

                                                        BFDBG_PRINTF("ReadFile: subfile=%d, invoking intermediate callback %p (user callback=%p preserved)",
                                                                subfileIndex, (void *)callback, (void *)data.currSlot.callbackFuncPtr);

                                                        callback(&data.currSlot);
                                                }

                                                /* Trim allocation to actual size when the
                                                   caller didn't provide a buffer. */
                                                if ((callback == NULL) && (originalDst == NULL))
                                                {
                                                        MEMPACK_ReallocMem(*sizePtr);
                                                }

                                                return ptrDst;
                                        }

                                        /* Unpacked read failed — fall through to the
                                           packed path as a last resort. */
                                        BFDBG_PRINTF_ERR("ReadFile: subfile=%d, UNPACKED READ FAILED — falling back to packed path",
                                                subfileIndex);
                                        *sizePtr = eSize; /* restore original size */
                                        if (originalDst == NULL && ptrDst != NULL)
                                        {
                                                /* We allocated memory above but the read
                                                   failed.  Free it so the packed path can
                                                   re-allocate with the correct size. */
                                                MEMPACK_ReallocMem(0);
                                                ptrDst = NULL;
                                        }
                                }
                                else
                                {
                                        BFDBG_PRINTF_ERR("ReadFile: subfile=%d, unpacked override exists but size <= 0 (%d)",
                                                subfileIndex, unpackedSize);
                                }
                        }
                        else
                        {
                                BFDBG_PRINTF("ReadFile: subfile=%d, no unpacked override — using packed path",
                                        subfileIndex);
                        }
                }
                else
                {
                        BFDBG_PRINTF("ReadFile: subfile=%d, PACKED mode — using CdRead", subfileIndex);
                }
        }
#endif /* CTR_NATIVE */

        BFDBG_PRINTF("ReadFile: subfile=%d, CdRead path — cdpos=%d + offset=0x%X",
                subfileIndex, bigfile->cdpos, eOffs);

        CdIntToPos(bigfile->cdpos + eOffs, &cdLoc);

        struct LoadQueueSlot *lqs = &data.currSlot;
        originalDst = ptrDst;
        sectorCount = (eSize + 0x7ffU) >> 0xb;
        readComplete = 1;

        // If no address given, then find one.
        if (ptrDst == NULL)
        {
#if defined(CTR_NATIVE)
                lqs->flags |= LT_MEMPACK;
#else
                lqs->flags |= LT_SETADDR;
#endif

                // allocate room for all sectors,
                // remove alignment before next Read
                sectorSize = sectorCount << 0xb;
                ptrDst = (void *)MEMPACK_AllocMem(sectorSize); // "FILE"
                if (ptrDst == NULL)
                {
                        BFDBG_PRINTF_ERR("ReadFile: subfile=%d, MEMPACK_AllocMem(%d) FAILED for CdRead",
                                subfileIndex, sectorSize);
                        return NULL;
                }
                BFDBG_PRINTF("ReadFile: subfile=%d, allocated %d bytes at %p for CdRead",
                        subfileIndex, sectorSize, ptrDst);
        }
        else
        {
#if defined(CTR_NATIVE)
                lqs->flags &= ~LT_MEMPACK;
#else
                lqs->flags &= ~LT_SETADDR;
#endif
        }

#if defined(CTR_NATIVE)
        // NOTE(aalhendi): native CD reads can call back before wrapper callers store
        // the returned pointer back into data.currSlot.
        lqs->ptrDestination = ptrDst;
        lqs->size_UNUSED = eSize;
#endif

        BFDBG_PRINTF("ReadFile: subfile=%d, CdRead — %d sectors to %p...",
                subfileIndex, sectorCount, ptrDst);

        while (1)
        {
                uVar5 = CdControl(CdlSetloc, &cdLoc, &paramOutput[0]);

                if (callback != NULL)
                {
                        sdata->callbackCdReadSuccess = callback;
                        CdReadCallback(LOAD_ReadFileASyncCallback);
                }
                else
                {
                        sdata->callbackCdReadSuccess = NULL;
                        CdReadCallback(NULL);
                }

                uVar5 &= CdRead(sectorCount, ptrDst, 0x80);

                if (callback == NULL)
                {
                        // Wait for all sectors to finish
                        readComplete = CdReadSync(0, (u8 *)0x0) < 1;
                }

                // If either command failed, or sync read did not finish, retry.
                if ((uVar5 != 0) && (readComplete != 0))
                        break;
        }

        if ((callback == NULL) && (originalDst == NULL))
        {
                MEMPACK_ReallocMem(*sizePtr);
        }

        BFDBG_PRINTF("ReadFile: subfile=%d, CdRead DONE — %d bytes", subfileIndex, *sizePtr);
        return ptrDst;
}
