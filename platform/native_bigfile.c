/**
 * platform/native_bigfile.c
 *
 * Dual-mode BIGFILE reading for the CTR PC native port.
 *
 * This module enables the game to read its asset data from either the
 * original packed BIGFILE.BIG archive or from an unpacked BIGFILE/
 * folder (the format produced by the community-standard CTR-tools
 * "bigtool" extractor).
 *
 * Three modes are supported:
 *
 *   PACKED   - Only BIGFILE.BIG exists.  All reads go through the
 *              existing CdControl/CdRead path (no change from stock).
 *
 *   UNPACKED - Only the BIGFILE/ folder exists.  The BigHeader +
 *              BigEntry[] directory is synthesised from BIGFILE.TXT.
 *              Subfile data is read directly from individual files.
 *
 *   HYBRID   - Both exist.  The directory is read from BIGFILE.BIG
 *              (preserving exact sector offsets for the packed path),
 *              but each subfile read first checks the BIGFILE/ folder
 *              for an override.  If the file exists there it is used;
 *              otherwise the read falls back to BIGFILE.BIG.
 *              This is the most convenient mode for modding: extract
 *              only the files you want to modify into BIGFILE/ and
 *              leave the rest packed.
 */

#include <macros.h>
#include <platform/native_bigfile.h>
#include <platform/native_assets.h>
#include <psx/types.h>

/* We need BigHeader / BigEntry layout but cannot include the full
   namespace_Load.h here (it pulls in game-level definitions).
   Instead we duplicate the minimal struct definitions; they are
   ABI-stable because the game writes them into shared memory. */
struct NativeBF_BigHeader
{
        int cdpos;
        int numEntry;
        /* struct NativeBF_BigEntry entries[]; */
};

struct NativeBF_BigEntry
{
        int offset; /* sector offset within BIGFILE.BIG, or subfile index in unpacked mode */
        int size;   /* byte count */
};

#define NATIVEBF_GETENTRY(h) ((struct NativeBF_BigEntry *)((char *)(h) + sizeof(struct NativeBF_BigHeader)))

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#define NATIVEBF_PATH_MAX       1024
#define NATIVEBF_DIR_NAME       "BIGFILE"
#define NATIVEBF_FILE_LIST      "BIGFILE.TXT"
#define NATIVEBF_BIGFILE_BIG    "BIGFILE.BIG"
#define NATIVEBF_SECTOR_SIZE    0x800
#define NATIVEBF_MAX_LINE       512

/* ------------------------------------------------------------------ */
/* Debug logging                                                      */
/* ------------------------------------------------------------------ */

/**
 * BFDBG / BFDBG_ERR — buffered-debug macros.
 *
 * Every message is fflush'd immediately so that if the game
 * freezes/crashes the last line printed is still visible in
 * the terminal.
 *
 * All messages use the "[BF-DBG]" prefix so you can grep or
 * filter the output easily:
 *
 *   ./ctr_native 2>&1 | grep "\[BF-DBG\]"
 */

#define BFDBG(fmt, ...) \
        do { fprintf(stdout, "[BF-DBG] " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while (0)

#define BFDBG_ERR(fmt, ...) \
        do { fprintf(stderr, "[BF-DBG] ERROR: " fmt "\n", ##__VA_ARGS__); fflush(stderr); } while (0)

/* ------------------------------------------------------------------ */
/* Internal types                                                     */
/* ------------------------------------------------------------------ */

/** One entry in the index-to-path mapping table. */
struct NativeBF_PathEntry
{
        char relPath[NATIVEBF_PATH_MAX]; /* normalised forward-slash path within BIGFILE/ */
};

/* ------------------------------------------------------------------ */
/* Module state                                                       */
/* ------------------------------------------------------------------ */

global_variable int s_bfMode = -1; /* -1 = not yet initialised */

/** Index -> path mapping (populated from BIGFILE.TXT). */
global_variable struct NativeBF_PathEntry *s_bfPaths = NULL;
global_variable int s_bfPathCount = 0;

/** Cached BIGFILE/ folder absolute path. */
global_variable char s_bfDirAbs[NATIVEBF_PATH_MAX] = {0};

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/** Return 1 if @p path is an existing directory, 0 otherwise. */
internal int NativeBF_DirExists(const char *path)
{
        struct stat st;
        return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/** Return 1 if @p path is an existing regular file, 0 otherwise. */
internal int NativeBF_FileExists(const char *path)
{
        struct stat st;
        return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

/** Strip trailing CR/LF/spaces from a line in-place. */
internal void NativeBF_StripTrailing(char *line)
{
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' ||
                           line[len - 1] == '\r' ||
                           line[len - 1] == ' '  ||
                           line[len - 1] == '\t'))
                line[--len] = '\0';
}

/** Replace backslashes with forward slashes in-place. */
internal void NativeBF_NormalizeSlashes(char *path)
{
        for (; *path; path++)
                if (*path == '\\')
                        *path = '/';
}

/**
 * Parse a single line from BIGFILE.TXT.
 *
 * Two formats are supported:
 *   Plain:    levels/tracks/proto8/1P/data.vrm
 *   Indexed:  000=levels\tracks\proto8\1P\data.vrm
 *
 * Lines starting with '#' are treated as comments and skipped.
 * Empty lines are skipped.
 *
 * The extracted path is normalised (backslashes -> forward slashes)
 * and written to @p outPath.
 *
 * Returns 1 if a valid path was extracted, 0 if the line should be
 * skipped.
 */
internal int NativeBF_ParseLine(char *line, char *outPath, size_t outPathSize)
{
        char *pathStart;

        NativeBF_StripTrailing(line);

        /* Skip empty lines and comments */
        if (line[0] == '\0' || line[0] == '#')
                return 0;

        pathStart = line;

        /* Detect "NNN=path" format used by some CTR-tools versions. */
        {
                char *eq = strchr(line, '=');
                if (eq != NULL)
                {
                        /* Verify everything before '=' is digits. */
                        int allDigits = 1;
                        char *p;
                        for (p = line; p < eq; p++)
                        {
                                if (*p < '0' || *p > '9')
                                {
                                        allDigits = 0;
                                        break;
                                }
                        }
                        if (allDigits && eq[1] != '\0')
                                pathStart = eq + 1;
                }
        }

        /* Copy and normalise */
        strncpy(outPath, pathStart, outPathSize - 1);
        outPath[outPathSize - 1] = '\0';
        NativeBF_NormalizeSlashes(outPath);

        return (outPath[0] != '\0');
}

/**
 * Build the full filesystem path for a BIGFILE/ subfile.
 * Returns the number of characters written (excluding NUL), or 0 on
 * error (buffer too small, etc.).
 */
internal int NativeBF_BuildFullPath(int index, char *dst, size_t dstSize)
{
        if (s_bfPaths == NULL || index < 0 || index >= s_bfPathCount)
                return 0;

        if (s_bfPaths[index].relPath[0] == '\0')
                return 0;

        int written = snprintf(dst, dstSize, "%s/%s",
                               s_bfDirAbs, s_bfPaths[index].relPath);

        if (written <= 0 || (size_t)written >= dstSize)
                return 0;

        return written;
}

/**
 * Internal: read BIGFILE.TXT and populate the path mapping.
 *
 * @param fillEntries  If non-NULL, also fill a BigEntry[] array at
 *                     fillEntries with each file's offset (=index) and
 *                     size (from stat).
 * @return Number of entries mapped, or -1 on error.
 */
internal int NativeBF_ReadFileList(struct NativeBF_BigEntry *fillEntries)
{
        char fileListPath[NATIVEBF_PATH_MAX];
        FILE *f;
        char line[NATIVEBF_MAX_LINE];
        int count;
        int missingCount = 0;

        BFDBG("ReadFileList: start (fillEntries=%s)",
                fillEntries ? "yes" : "no");

        if (s_bfDirAbs[0] == '\0')
        {
                BFDBG_ERR("ReadFileList: s_bfDirAbs is empty, cannot locate BIGFILE/");
                return -1;
        }

        BFDBG("ReadFileList: BIGFILE dir = %s", s_bfDirAbs);

        /* Try BIGFILE.TXT inside the BIGFILE/ folder first,
           then at the assets root. */
        {
                int written = snprintf(fileListPath, sizeof(fileListPath),
                                       "%s/%s", s_bfDirAbs, NATIVEBF_FILE_LIST);
                if (written <= 0 || (size_t)written >= sizeof(fileListPath))
                {
                        BFDBG_ERR("ReadFileList: path too long for BIGFILE.TXT in folder");
                        return -1;
                }

                BFDBG("ReadFileList: trying %s", fileListPath);

                if (!NativeBF_FileExists(fileListPath))
                {
                        BFDBG("ReadFileList: NOT found in folder, trying assets root...");
                        /* Fallback: assets/BIGFILE.TXT */
                        if (!NativeAssets_BuildPath(NATIVEBF_FILE_LIST,
                                                    fileListPath, sizeof(fileListPath)))
                        {
                                BFDBG_ERR("ReadFileList: BuildPath failed for BIGFILE.TXT");
                                return -1;
                        }
                        BFDBG("ReadFileList: fallback path = %s", fileListPath);
                }
        }

        if (!NativeBF_FileExists(fileListPath))
        {
                BFDBG_ERR("ReadFileList: BIGFILE.TXT not found at %s", fileListPath);
                return -1;
        }

        f = fopen(fileListPath, "r");
        if (!f)
        {
                BFDBG_ERR("ReadFileList: fopen failed for %s", fileListPath);
                return -1;
        }

        BFDBG("ReadFileList: opened %s, counting entries (pass 1)...", fileListPath);

        /* ---- First pass: count valid entries ---- */
        count = 0;
        while (fgets(line, sizeof(line), f))
        {
                char tmp[NATIVEBF_PATH_MAX];
                if (NativeBF_ParseLine(line, tmp, sizeof(tmp)))
                        count++;
        }

        if (count == 0)
        {
                fclose(f);
                BFDBG_ERR("ReadFileList: BIGFILE.TXT contains no valid entries");
                return -1;
        }

        BFDBG("ReadFileList: found %d entries, allocating path table...", count);

        /* ---- Allocate path array ---- */
        if (s_bfPaths)
                free(s_bfPaths);

        s_bfPaths = (struct NativeBF_PathEntry *)malloc(
                sizeof(struct NativeBF_PathEntry) * (size_t)count);
        if (!s_bfPaths)
        {
                fclose(f);
                BFDBG_ERR("ReadFileList: malloc failed for %d entries", count);
                return -1;
        }
        s_bfPathCount = 0;

        /* ---- Second pass: fill path array and (optionally) BigEntry[] ---- */
        BFDBG("ReadFileList: filling entries (pass 2)...");
        rewind(f);
        {
                int idx = 0;
                while (fgets(line, sizeof(line), f) && idx < count)
                {
                        char relPath[NATIVEBF_PATH_MAX];
                        char fullPath[NATIVEBF_PATH_MAX];
                        struct stat st;

                        if (!NativeBF_ParseLine(line, relPath, sizeof(relPath)))
                                continue;

                        /* Store the normalised relative path. */
                        strncpy(s_bfPaths[idx].relPath, relPath,
                                NATIVEBF_PATH_MAX - 1);
                        s_bfPaths[idx].relPath[NATIVEBF_PATH_MAX - 1] = '\0';

                        /* Optionally fill BigEntry for the caller. */
                        if (fillEntries)
                        {
                                int written = snprintf(fullPath, sizeof(fullPath),
                                                       "%s/%s", s_bfDirAbs, relPath);
                                if (written > 0 && (size_t)written < sizeof(fullPath) &&
                                    stat(fullPath, &st) == 0 && S_ISREG(st.st_mode))
                                {
                                        fillEntries[idx].offset = idx; /* index as identifier */
                                        fillEntries[idx].size   = (int)st.st_size;
                                }
                                else
                                {
                                        fillEntries[idx].offset = idx;
                                        fillEntries[idx].size   = 0; /* missing */
                                        missingCount++;
                                        BFDBG_ERR("ReadFileList: missing file #%d: %s (full: %s)",
                                                idx, relPath, fullPath);
                                }
                        }

                        idx++;
                }
                s_bfPathCount = idx;
        }

        fclose(f);

        BFDBG("ReadFileList: done — %d entries mapped, %d missing",
                s_bfPathCount, missingCount);

        /* Log first few entries for verification */
        {
                int showCount = s_bfPathCount < 5 ? s_bfPathCount : 5;
                int i;
                BFDBG("ReadFileList: first %d paths:", showCount);
                for (i = 0; i < showCount; i++)
                        BFDBG("  [%d] %s", i, s_bfPaths[i].relPath);
                if (s_bfPathCount > 5)
                        BFDBG("  ... (%d more)", s_bfPathCount - 5);
        }

        return s_bfPathCount;
}

/* ================================================================== */
/* Public API                                                         */
/* ================================================================== */

int NativeBigfile_Init(void)
{
        char bigfilePath[NATIVEBF_PATH_MAX];
        char bigfileDir[NATIVEBF_PATH_MAX];
        int hasPacked, hasUnpacked;

        BFDBG("Init: starting bigfile detection...");

        if (s_bfMode >= 0)
        {
                BFDBG("Init: already initialised (mode=%d), skipping", s_bfMode);
                return 1; /* already initialised */
        }

        /* Check for unpacked BIGFILE/ folder. */
        hasUnpacked = 0;
        if (NativeAssets_BuildPath(NATIVEBF_DIR_NAME, bigfileDir, sizeof(bigfileDir)))
        {
                BFDBG("Init: checking for BIGFILE/ folder at: %s", bigfileDir);
                hasUnpacked = NativeBF_DirExists(bigfileDir);
                BFDBG("Init: BIGFILE/ folder %s", hasUnpacked ? "FOUND" : "not found");
                if (hasUnpacked)
                {
                        strncpy(s_bfDirAbs, bigfileDir, NATIVEBF_PATH_MAX - 1);
                        s_bfDirAbs[NATIVEBF_PATH_MAX - 1] = '\0';
                }
        }
        else
        {
                BFDBG_ERR("Init: BuildPath failed for BIGFILE dir name");
        }

        /* Check for packed BIGFILE.BIG. */
        hasPacked = 0;
        if (NativeAssets_BuildPath(NATIVEBF_BIGFILE_BIG, bigfilePath, sizeof(bigfilePath)))
        {
                BFDBG("Init: checking for BIGFILE.BIG at: %s", bigfilePath);
                hasPacked = NativeBF_FileExists(bigfilePath);
                BFDBG("Init: BIGFILE.BIG %s", hasPacked ? "FOUND" : "not found");
        }
        else
        {
                BFDBG_ERR("Init: BuildPath failed for BIGFILE.BIG");
        }

        /* Determine mode. */
        if (hasUnpacked && hasPacked)
        {
                s_bfMode = NATIVE_BIGFILE_MODE_HYBRID;
                BFDBG("Init: mode = HYBRID (%s + %s)", bigfileDir, bigfilePath);
        }
        else if (hasUnpacked)
        {
                s_bfMode = NATIVE_BIGFILE_MODE_UNPACKED;
                BFDBG("Init: mode = UNPACKED (%s)", bigfileDir);
        }
        else if (hasPacked)
        {
                s_bfMode = NATIVE_BIGFILE_MODE_PACKED;
                BFDBG("Init: mode = PACKED (%s)", bigfilePath);
        }
        else
        {
                s_bfMode = NATIVE_BIGFILE_MODE_PACKED; /* fallback */
                BFDBG_ERR("Init: NEITHER BIGFILE.BIG nor BIGFILE/ folder found!");
                BFDBG_ERR("Init: checked dir: %s", bigfileDir);
                BFDBG_ERR("Init: checked file: %s", bigfilePath);
                return 0;
        }

        BFDBG("Init: complete (mode=%d)", s_bfMode);
        return 1;
}

int NativeBigfile_GetMode(void)
{
        if (s_bfMode < 0)
        {
                BFDBG("GetMode: not yet initialised, calling Init...");
                NativeBigfile_Init();
        }
        return s_bfMode;
}

int NativeBigfile_BuildDirectory(void *buffer, int bufferSize)
{
        struct NativeBF_BigHeader *bh;
        struct NativeBF_BigEntry  *entries;
        int maxEntries;
        int count;

        BFDBG("BuildDirectory: start (buffer=%p, bufferSize=0x%X)", buffer, bufferSize);

        if (buffer == NULL || bufferSize < (int)sizeof(struct NativeBF_BigHeader))
        {
                BFDBG_ERR("BuildDirectory: bad params (buffer=%p, size=%d, need >= %d)",
                        buffer, bufferSize, (int)sizeof(struct NativeBF_BigHeader));
                return -1;
        }

        /* Ensure the folder path is cached. */
        if (s_bfDirAbs[0] == '\0')
        {
                char bigfileDir[NATIVEBF_PATH_MAX];
                BFDBG("BuildDirectory: s_bfDirAbs empty, resolving...");
                if (!NativeAssets_BuildPath(NATIVEBF_DIR_NAME,
                                            bigfileDir, sizeof(bigfileDir)))
                {
                        BFDBG_ERR("BuildDirectory: BuildPath failed for BIGFILE dir");
                        return -1;
                }
                strncpy(s_bfDirAbs, bigfileDir, NATIVEBF_PATH_MAX - 1);
                s_bfDirAbs[NATIVEBF_PATH_MAX - 1] = '\0';
                BFDBG("BuildDirectory: resolved BIGFILE dir = %s", s_bfDirAbs);
        }

        bh      = (struct NativeBF_BigHeader *)buffer;
        entries = NATIVEBF_GETENTRY(bh);

        maxEntries = (bufferSize - (int)sizeof(struct NativeBF_BigHeader))
                   / (int)sizeof(struct NativeBF_BigEntry);

        BFDBG("BuildDirectory: maxEntries = %d, reading file list...", maxEntries);

        count = NativeBF_ReadFileList(entries);
        if (count < 0)
        {
                BFDBG_ERR("BuildDirectory: ReadFileList failed");
                return -1;
        }

        if (count > maxEntries)
        {
                BFDBG_ERR("BuildDirectory: too many entries (%d) for buffer (max %d)",
                        count, maxEntries);
                return -1;
        }

        /* Fill BigHeader. */
        bh->cdpos    = 0; /* no physical CD position in unpacked mode */
        bh->numEntry = count;

        BFDBG("BuildDirectory: SUCCESS — %d entries, header at cdpos=0", count);
        return count;
}

int NativeBigfile_LoadPathMap(void)
{
        BFDBG("LoadPathMap: start (hybrid mode override mapping)");

        /* Ensure the folder path is cached. */
        if (s_bfDirAbs[0] == '\0')
        {
                char bigfileDir[NATIVEBF_PATH_MAX];
                BFDBG("LoadPathMap: s_bfDirAbs empty, resolving...");
                if (!NativeAssets_BuildPath(NATIVEBF_DIR_NAME,
                                            bigfileDir, sizeof(bigfileDir)))
                {
                        BFDBG_ERR("LoadPathMap: BuildPath failed for BIGFILE dir");
                        return -1;
                }
                strncpy(s_bfDirAbs, bigfileDir, NATIVEBF_PATH_MAX - 1);
                s_bfDirAbs[NATIVEBF_PATH_MAX - 1] = '\0';
                BFDBG("LoadPathMap: resolved BIGFILE dir = %s", s_bfDirAbs);
        }

        /* Read the file list without filling BigEntry[]. */
        int count = NativeBF_ReadFileList(NULL);
        if (count < 0)
        {
                BFDBG_ERR("LoadPathMap: could not load BIGFILE.TXT — folder overrides DISABLED");
                return -1;
        }

        BFDBG("LoadPathMap: SUCCESS — %d entries for hybrid override", count);
        return count;
}

int NativeBigfile_HasUnpackedFile(int index)
{
        char fullPath[NATIVEBF_PATH_MAX];

        if (s_bfPaths == NULL || index < 0 || index >= s_bfPathCount)
        {
                BFDBG_ERR("HasUnpackedFile: bad index %d (pathCount=%d)",
                        index, s_bfPathCount);
                return 0;
        }

        if (!NativeBF_BuildFullPath(index, fullPath, sizeof(fullPath)))
        {
                BFDBG_ERR("HasUnpackedFile: BuildFullPath failed for index %d", index);
                return 0;
        }

        if (NativeBF_FileExists(fullPath))
        {
                BFDBG("HasUnpackedFile: [%d] FOUND — %s", index,
                        s_bfPaths[index].relPath);
                return 1;
        }

        BFDBG("HasUnpackedFile: [%d] NOT FOUND — %s", index, fullPath);
        return 0;
}

int NativeBigfile_GetSubfileSize(int index)
{
        char fullPath[NATIVEBF_PATH_MAX];
        struct stat st;

        if (s_bfPaths == NULL || index < 0 || index >= s_bfPathCount)
        {
                BFDBG_ERR("GetSubfileSize: bad index %d (pathCount=%d)",
                        index, s_bfPathCount);
                return -1;
        }

        if (!NativeBF_BuildFullPath(index, fullPath, sizeof(fullPath)))
        {
                BFDBG_ERR("GetSubfileSize: BuildFullPath failed for index %d", index);
                return -1;
        }

        if (stat(fullPath, &st) != 0 || !S_ISREG(st.st_mode))
        {
                BFDBG_ERR("GetSubfileSize: stat failed or not a file: %s", fullPath);
                return -1;
        }

        BFDBG("GetSubfileSize: [%d] %d bytes — %s",
                index, (int)st.st_size, s_bfPaths[index].relPath);
        return (int)st.st_size;
}

int NativeBigfile_ReadSubfile(int index, void *dst, int dstSize)
{
        char fullPath[NATIVEBF_PATH_MAX];
        FILE *f;
        long fileSize;
        size_t bytesRead;

        BFDBG("ReadSubfile: [%d] start (dst=%p, dstSize=0x%X)",
                index, dst, dstSize);

        if (dst == NULL || dstSize <= 0)
        {
                BFDBG_ERR("ReadSubfile: bad params (dst=%p, dstSize=%d)", dst, dstSize);
                return -1;
        }

        if (s_bfPaths == NULL || index < 0 || index >= s_bfPathCount)
        {
                BFDBG_ERR("ReadSubfile: bad index %d (pathCount=%d)",
                        index, s_bfPathCount);
                return -1;
        }

        if (!NativeBF_BuildFullPath(index, fullPath, sizeof(fullPath)))
        {
                BFDBG_ERR("ReadSubfile: BuildFullPath failed for index %d", index);
                return -1;
        }

        BFDBG("ReadSubfile: [%d] opening %s", index, fullPath);

        f = fopen(fullPath, "rb");
        if (!f)
        {
                BFDBG_ERR("ReadSubfile: [%d] FOPEN FAILED — %s", index, fullPath);
                return -1;
        }

        /* Determine file size. */
        fseek(f, 0, SEEK_END);
        fileSize = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (fileSize <= 0)
        {
                fclose(f);
                BFDBG_ERR("ReadSubfile: [%d] file size <= 0 (%ld) — %s",
                        index, fileSize, fullPath);
                return -1;
        }

        /* Don't overflow the caller's buffer. */
        if ((int)fileSize > dstSize)
        {
                BFDBG("ReadSubfile: [%d] file %ld bytes > buffer %d bytes, clamping",
                        index, fileSize, dstSize);
                fileSize = (long)dstSize;
        }

        BFDBG("ReadSubfile: [%d] reading %ld bytes from %s",
                index, fileSize, s_bfPaths[index].relPath);

        bytesRead = fread(dst, 1, (size_t)fileSize, f);
        fclose(f);

        if ((long)bytesRead != fileSize)
        {
                BFDBG_ERR("ReadSubfile: [%d] SHORT READ — expected %ld, got %zu — %s",
                        index, fileSize, bytesRead, fullPath);
                return -1;
        }

        BFDBG("ReadSubfile: [%d] OK — %zu bytes read", index, bytesRead);
        return (int)bytesRead;
}
