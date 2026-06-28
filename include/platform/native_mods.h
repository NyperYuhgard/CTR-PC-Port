#ifndef NATIVE_MODS_H
#define NATIVE_MODS_H

#include <stddef.h>
#include <stdio.h>

#define NATIVE_MODS_MAX_MODS       64
#define NATIVE_MODS_MAX_HOOKS      128
#define NATIVE_MODS_NAME_MAX       64
#define NATIVE_MODS_PATH_MAX       1024

enum NativeModHookType
{
    NATIVE_MOD_HOOK_ON_INIT,
    NATIVE_MOD_HOOK_ON_UPDATE,
    NATIVE_MOD_HOOK_ON_RENDER,
    NATIVE_MOD_HOOK_ON_INPUT,
    NATIVE_MOD_HOOK_ON_TITLE_INIT,
    NATIVE_MOD_HOOK_ON_FILE_OPEN,

    /* Granular game-function hooks */
    NATIVE_MOD_HOOK_ON_FIRE_PRE,
    NATIVE_MOD_HOOK_ON_FIRE_POST,
    NATIVE_MOD_HOOK_ON_COLLIDE_PRE,
    NATIVE_MOD_HOOK_ON_COLLIDE_POST,
    NATIVE_MOD_HOOK_ON_GRAVITY_PRE,

    NATIVE_MOD_HOOK_COUNT
};

struct NativeModInfo
{
    char name[NATIVE_MODS_NAME_MAX];
    char path[NATIVE_MODS_PATH_MAX];
    int enabled;
};

/* ============================================================
 * Core mod engine lifecycle
 * ============================================================ */

int  NativeMods_Init(void);
int  NativeMods_ScanMods(void);
int  NativeMods_GetModCount(void);
const struct NativeModInfo *NativeMods_GetMod(int index);
int  NativeMods_ToggleMod(int index);
int  NativeMods_IsModEnabled(int index);

const char *NativeMods_GetModsDir(void);

int  NativeMods_LoadModScripts(void);
void NativeMods_CallHook(enum NativeModHookType hook);
void NativeMods_CallHookWithDelta(enum NativeModHookType hook, int dt);

/* ============================================================
 * Game state cache — call from game loop before firing hooks
 * ============================================================ */

void NativeMods_CacheGameState(void);

/* ============================================================
 * Draw queue flush — call from render path to execute
 * queued drawing commands from Lua mods
 * ============================================================ */

void NativeMods_FlushDrawQueue(void);

/* ============================================================
 * Language and file overrides
 * ============================================================ */

FILE *NativeMods_OpenFile(const char *relativePath, const char *mode);
void NativeMods_OnLanguageLoaded(char **lngStrings, int numStrings);

/* ============================================================
 * Hook context — set before calling CallHook from game code
 * ============================================================ */

void NativeMods_SetHookContext(int driverIndex, int arg0, int arg1, int arg2, int arg3);
int  NativeMods_FindDriverIndex(void *driver);

#endif
