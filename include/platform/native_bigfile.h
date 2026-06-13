#ifndef NATIVE_BIGFILE_H
#define NATIVE_BIGFILE_H

/**
 * Dual-mode BIGFILE reading for CTR Native.
 *
 * Supports three modes:
 *   PACKED   – Read everything from the original BIGFILE.BIG archive.
 *   UNPACKED – Read directory from BIGFILE.TXT and data from individual
 *              files in the BIGFILE/ folder (CTR-tools extraction format).
 *   HYBRID   – Read directory from BIGFILE.BIG, but check the BIGFILE/
 *              folder for individual file overrides before falling back to
 *              the packed archive.
 *
 * The unpacked folder layout follows CTR-tools "bigtool" output:
 *
 *   assets/
 *     BIGFILE.BIG          <- packed archive (optional in UNPACKED mode)
 *     BIGFILE.TXT          <- file list, one relative path per line
 *     BIGFILE/
 *       levels/
 *         tracks/
 *           proto8/
 *             1P/
 *               data.vrm
 *               data.lev
 *         ...
 *       models/
 *         racers/
 *           hi/
 *             crash.ctr
 *         ...
 *       lang/
 *         en.lng
 *         ...
 *
 * BIGFILE.TXT format (two supported styles):
 *   Plain:    levels/tracks/proto8/1P/data.vrm
 *   Indexed:  000=levels\tracks\proto8\1P\data.vrm
 *
 * Backslash paths are normalised to forward slashes automatically.
 */

#include <stddef.h>
#include <stdio.h>

/* ================================================================== */
/* Bigfile debug logging macros                                       */
/* ================================================================== */
/**
 * These macros are used throughout the bigfile loading path
 * (native_bigfile.c, LOAD_15_ReadDirectory.c, LOAD_21_ReadFile.c)
 * so that every step is visible when running from a terminal.
 *
 * All output is fflush'd immediately — if the game freezes,
 * the last line printed is still visible.
 *
 * Filter output in terminal:
 *   ./ctr_native 2>&1 | grep "\[BF-DBG\]"
 **/

#define BFDBG_PRINTF(fmt, ...) \
        do { fprintf(stdout, "[BF-DBG] " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while (0)

#define BFDBG_PRINTF_ERR(fmt, ...) \
        do { fprintf(stderr, "[BF-DBG] ERROR: " fmt "\n", ##__VA_ARGS__); fflush(stderr); } while (0)

/* Bigfile reading modes */
enum NativeBigfileMode
{
        NATIVE_BIGFILE_MODE_PACKED   = 0, /* only BIGFILE.BIG exists            */
        NATIVE_BIGFILE_MODE_UNPACKED = 1, /* only BIGFILE/ folder exists        */
        NATIVE_BIGFILE_MODE_HYBRID   = 2  /* both exist - folder overrides .BIG */
};

/**
 * Initialise the bigfile sub-system.
 * Detects which mode to use and caches the result.
 * Safe to call multiple times; only the first call has effect.
 * Returns 1 on success, 0 if no bigfile source could be found.
 */
int NativeBigfile_Init(void);

/**
 * Returns the current bigfile mode.
 * Calls NativeBigfile_Init() lazily if needed.
 */
int NativeBigfile_GetMode(void);

/**
 * Build a BigHeader + BigEntry[] directory in @p buffer from BIGFILE.TXT.
 * Used in UNPACKED mode - replaces the CdRead-based directory loading.
 *
 * @param buffer      Destination buffer (at least 0x4000 bytes, same as
 *                    the original LOAD_ReadDirectory allocation).
 * @param bufferSize  Size of @p buffer in bytes.
 * @return Number of entries on success, -1 on error.
 */
int NativeBigfile_BuildDirectory(void *buffer, int bufferSize);

/**
 * Load the index -> relative-path mapping from BIGFILE.TXT without
 * touching the BigEntry array.
 * Used in HYBRID mode where the directory comes from BIGFILE.BIG but
 * we still need to know which files exist in BIGFILE/ for overrides.
 *
 * @return Number of entries mapped on success, -1 on error.
 */
int NativeBigfile_LoadPathMap(void);

/**
 * Check whether an unpacked override exists for subfile @p index.
 * Returns 1 if a readable file exists in the BIGFILE/ folder,
 * 0 otherwise.
 */
int NativeBigfile_HasUnpackedFile(int index);

/**
 * Get the file size (in bytes) of an unpacked subfile.
 * Returns the size, or -1 if the file doesn't exist.
 */
int NativeBigfile_GetSubfileSize(int index);

/**
 * Read an unpacked subfile into @p dst.
 *
 * @param index   Subfile index (matches BigEntry array order).
 * @param dst     Destination buffer (must be large enough for the file).
 * @param dstSize Maximum number of bytes to write into @p dst.
 * @return Number of bytes actually read, or -1 on error.
 */
int NativeBigfile_ReadSubfile(int index, void *dst, int dstSize);

#endif /* NATIVE_BIGFILE_H */
