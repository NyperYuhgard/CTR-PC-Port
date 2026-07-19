#include <macros.h>
#include <platform/native_mods.h>
#include <platform/native_assets.h>
#include <psx/types.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* Ensure RECT type is available for CTR_Box_Draw* functions.
 *
 * In main.c, the game code uses #define RECT RECT16 before including
 * game headers, and #undef RECT after.  native_mods.c is included
 * after the #undef, so RECT may not be defined.  However, RECT16 is
 * always available from psx/libgpu.h (included in main.c before us).
 *
 * We define RECT as RECT16 if it's not already defined, which ensures
 * compatibility with CTR's function declarations without creating a
 * conflicting type.
 */
#ifndef RECT
#define RECT RECT16
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define NATIVE_MODS_DIR_NAME         "mods"
#define NATIVE_MODS_SCRIPT_NAME      "main.lua"
#define NATIVE_MODS_FILES_DIR        "files"
#define NATIVE_MODS_BIGFILE_DIR      "BIGFILE"
#define NATIVE_MODS_STATE_FILE       "mods_state.cfg"

/* ============================================================
 * Driver struct offsets for Lua memory access
 * These must match namespace_Vehicle.h
 * ============================================================ */
#define DRIVER_OFFSET_RESERVES              0x3E2
#define DRIVER_OFFSET_FIRE_SPEED_CAP        0x3E4
#define DRIVER_OFFSET_TURBO_METER_ROOM_LEFT 0x3DC
#define DRIVER_OFFSET_TURBO_OUTSIDE_TIMER   0x3DE
#define DRIVER_OFFSET_NUM_TURBOS            0x614
#define DRIVER_OFFSET_CONST_SACRED_FIRE     0x432
#define DRIVER_OFFSET_CONST_SINGLE_TURBO    0x430
#define DRIVER_OFFSET_CONST_TURBO_MAX_ROOM  0x476
#define DRIVER_OFFSET_CONST_ACCEL_RESERVES  0x42A
#define DRIVER_OFFSET_DRIVER_ID             0x4A
#define DRIVER_OFFSET_KART_STATE            0x376
#define DRIVER_OFFSET_ACTIONS_FLAG_SET      0x2C8
#define DRIVER_OFFSET_SPEED_APPROX          0x38E
#define DRIVER_OFFSET_SPEED                 0x38C
#define DRIVER_OFFSET_BASE_SPEED            0x39C
#define DRIVER_OFFSET_FIRE_SPEED            0x39E

/* Extended driver fields */
#define DRIVER_OFFSET_LAP_TIME              0x40
#define DRIVER_OFFSET_LAP_INDEX             0x44
#define DRIVER_OFFSET_NUM_WUMPAS            0x30
#define DRIVER_OFFSET_POS_CURR              0x2D4  /* Vec3: s32[3] */
#define DRIVER_OFFSET_VELOCITY              0x88   /* Vec3: s32[3] */
#define DRIVER_OFFSET_RANK                  0x482
#define DRIVER_OFFSET_CONST_GRAVITY         0x416

struct NativeModsState
{
    lua_State *L;
    struct NativeModInfo mods[NATIVE_MODS_MAX_MODS];
    int modCount;
    int initialized;
    int luaRefs[NATIVE_MOD_HOOK_COUNT];
    char activeModName[NATIVE_MODS_NAME_MAX];
    char activeModPath[NATIVE_MODS_PATH_MAX];

    /* Cached game state for Lua rendering */
    void *cachedDriverPtrs[8];
    int  cachedNumPlayers;
    int  cachedGameMode1;

    /* Hook context — set before CallHook so Lua can retrieve args */
    struct
    {
        int driverIndex;
        int intArgs[4];
    } hookContext;
};

global_variable struct NativeModsState s_mods;

/* ============================================================
 * Forward declarations for game functions we need
 * These are declared in functions.h / common.h, which are
 * included via game_includes.h before this file in the unity build.
 * We only need the sdata extern declaration since it's
 * defined in regionsEXE.h.
 * ============================================================ */

extern struct sData *sdata;

internal int NativeMods_LuaPanic(lua_State *L)
{
    fprintf(stderr, "[Mods] Lua panic: %s\n", lua_tostring(L, -1));
    return 0;
}

internal void NativeMods_ReportLuaError(lua_State *L, const char *context)
{
    fprintf(stderr, "[Mods] Lua error in %s: %s\n", context, lua_tostring(L, -1));
    lua_pop(L, 1);
}

/* ============================================================
 * Existing Lua API functions
 * ============================================================ */

static int NativeMods_Lua_Log(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    fprintf(stdout, "[Mod] %s\n", msg ? msg : "(nil)");
    return 0;
}

static int NativeMods_Lua_GetModPath(lua_State *L)
{
    lua_pushstring(L, s_mods.activeModPath);
    return 1;
}

static int NativeMods_Lua_ReadFile(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    char fullPath[NATIVE_MODS_PATH_MAX];
    FILE *file;
    long size;
    char *buf;

    int written = snprintf(fullPath, sizeof(fullPath), "%s/%s/%s", s_mods.activeModPath, NATIVE_MODS_FILES_DIR, path);
    if ((written <= 0) || ((size_t)written >= sizeof(fullPath)))
    {
        lua_pushnil(L);
        return 1;
    }

    file = fopen(fullPath, "rb");
    if (!file)
    {
        lua_pushnil(L);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    buf = (char *)malloc((size_t)size + 1);
    if (!buf)
    {
        fclose(file);
        lua_pushnil(L);
        return 1;
    }

    size_t bytesRead = fread(buf, 1, (size_t)size, file);
    buf[bytesRead] = '\0';
    fclose(file);

    lua_pushlstring(L, buf, bytesRead);
    free(buf);
    return 1;
}

static int NativeMods_Lua_WriteFile(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    size_t dataLen;
    const char *data = luaL_checklstring(L, 2, &dataLen);
    char fullPath[NATIVE_MODS_PATH_MAX];
    FILE *file;

    int written = snprintf(fullPath, sizeof(fullPath), "%s/%s/%s", s_mods.activeModPath, NATIVE_MODS_FILES_DIR, path);
    if ((written <= 0) || ((size_t)written >= sizeof(fullPath)))
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    file = fopen(fullPath, "wb");
    if (!file)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    fwrite(data, 1, dataLen, file);
    fclose(file);
    lua_pushboolean(L, 1);
    return 1;
}

static int NativeMods_Lua_Hook(lua_State *L)
{
    const char *hookName = luaL_checkstring(L, 1);
    enum NativeModHookType hookType;

    if (!lua_isfunction(L, 2))
        return luaL_argerror(L, 2, "expected function");

    if (strcmp(hookName, "onInit") == 0)
        hookType = NATIVE_MOD_HOOK_ON_INIT;
    else if (strcmp(hookName, "onUpdate") == 0)
        hookType = NATIVE_MOD_HOOK_ON_UPDATE;
    else if (strcmp(hookName, "onRender") == 0)
        hookType = NATIVE_MOD_HOOK_ON_RENDER;
    else if (strcmp(hookName, "onInput") == 0)
        hookType = NATIVE_MOD_HOOK_ON_INPUT;
    else if (strcmp(hookName, "onTitleInit") == 0)
        hookType = NATIVE_MOD_HOOK_ON_TITLE_INIT;
    else if (strcmp(hookName, "onFileOpen") == 0)
        hookType = NATIVE_MOD_HOOK_ON_FILE_OPEN;
    else if (strcmp(hookName, "onFirePre") == 0)
        hookType = NATIVE_MOD_HOOK_ON_FIRE_PRE;
    else if (strcmp(hookName, "onFirePost") == 0)
        hookType = NATIVE_MOD_HOOK_ON_FIRE_POST;
    else if (strcmp(hookName, "onCollidePre") == 0)
        hookType = NATIVE_MOD_HOOK_ON_COLLIDE_PRE;
    else if (strcmp(hookName, "onCollidePost") == 0)
        hookType = NATIVE_MOD_HOOK_ON_COLLIDE_POST;
    else if (strcmp(hookName, "onGravityPre") == 0)
        hookType = NATIVE_MOD_HOOK_ON_GRAVITY_PRE;
    else
        return luaL_argerror(L, 1, "unknown hook name (onInit, onUpdate, onRender, onInput, onTitleInit, onFileOpen, onFirePre, onFirePost, onCollidePre, onCollidePost, onGravityPre)");

    lua_pushvalue(L, 2);
    s_mods.luaRefs[hookType] = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

/* ============================================================
 * NEW: Game memory access API
 * ============================================================ */

/* Helper: Read a signed 16-bit value from a memory address + offset */
static s16 NativeMods_ReadS16(void *base, int offset)
{
    if (!base)
        return 0;
    /* Handle potentially unaligned access */
    s16 value;
    memcpy(&value, ((char *)base) + offset, sizeof(s16));
    return value;
}

/* Helper: Read an unsigned 8-bit value from a memory address + offset */
static u8 NativeMods_ReadU8(void *base, int offset)
{
    if (!base)
        return 0;
    return (u8)(((char *)base)[offset]);
}

/* Helper: Read a signed 32-bit value from a memory address + offset */
static s32 NativeMods_ReadS32(void *base, int offset)
{
    if (!base)
        return 0;
    s32 value;
    memcpy(&value, ((char *)base) + offset, sizeof(s32));
    return value;
}

/* ============================================================
 * Write helpers — allow mods to modify game memory
 * ============================================================ */

/* Helper: Write a signed 16-bit value to a memory address + offset */
static void NativeMods_WriteS16(void *base, int offset, s16 value)
{
    if (!base)
        return;
    memcpy(((char *)base) + offset, &value, sizeof(s16));
}

/* Helper: Write an unsigned 8-bit value to a memory address + offset */
static void NativeMods_WriteU8(void *base, int offset, u8 value)
{
    if (!base)
        return;
    ((char *)base)[offset] = (char)value;
}

/* Helper: Write a signed 32-bit value to a memory address + offset */
static void NativeMods_WriteS32(void *base, int offset, s32 value)
{
    if (!base)
        return;
    memcpy(((char *)base) + offset, &value, sizeof(s32));
}

/* mod.getDriver(index) — Returns a table with driver fields for player index (0-7)
 *
 * Returns a Lua table:
 *   { reserves, fireSpeedCap, turbo_MeterRoomLeft, turbo_outsideTimer,
 *     numTurbos, const_SacredFireSpeed, const_SingleTurboSpeed,
 *     const_turboMaxRoom, driverID, kartState, actionsFlagSet, speedApprox,
 *     valid }
 * If the driver pointer is NULL, returns a table with valid=false.
 */
static int NativeMods_Lua_GetDriver(lua_State *L)
{
    int index = (int)luaL_checkinteger(L, 1);
    if (index < 0 || index > 7)
        return luaL_argerror(L, 1, "driver index must be 0-7");

    void *driverPtr = s_mods.cachedDriverPtrs[index];

    lua_newtable(L);

    if (!driverPtr)
    {
        lua_pushboolean(L, 0);
        lua_setfield(L, -2, "valid");
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "reserves");
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "fireSpeedCap");
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "turbo_MeterRoomLeft");
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "numTurbos");
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "const_SacredFireSpeed");
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "const_SingleTurboSpeed");
        return 1;
    }

    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "valid");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(driverPtr, DRIVER_OFFSET_RESERVES));
    lua_setfield(L, -2, "reserves");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(driverPtr, DRIVER_OFFSET_FIRE_SPEED_CAP));
    lua_setfield(L, -2, "fireSpeedCap");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(driverPtr, DRIVER_OFFSET_TURBO_METER_ROOM_LEFT));
    lua_setfield(L, -2, "turbo_MeterRoomLeft");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(driverPtr, DRIVER_OFFSET_TURBO_OUTSIDE_TIMER));
    lua_setfield(L, -2, "turbo_outsideTimer");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadU8(driverPtr, DRIVER_OFFSET_NUM_TURBOS));
    lua_setfield(L, -2, "numTurbos");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(driverPtr, DRIVER_OFFSET_CONST_SACRED_FIRE));
    lua_setfield(L, -2, "const_SacredFireSpeed");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(driverPtr, DRIVER_OFFSET_CONST_SINGLE_TURBO));
    lua_setfield(L, -2, "const_SingleTurboSpeed");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadU8(driverPtr, DRIVER_OFFSET_CONST_TURBO_MAX_ROOM));
    lua_setfield(L, -2, "const_turboMaxRoom");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadU8(driverPtr, DRIVER_OFFSET_DRIVER_ID));
    lua_setfield(L, -2, "driverID");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(driverPtr, DRIVER_OFFSET_ACTIONS_FLAG_SET));
    lua_setfield(L, -2, "actionsFlagSet");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(driverPtr, DRIVER_OFFSET_SPEED_APPROX));
    lua_setfield(L, -2, "speedApprox");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(driverPtr, DRIVER_OFFSET_SPEED));
    lua_setfield(L, -2, "speed");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(driverPtr, DRIVER_OFFSET_BASE_SPEED));
    lua_setfield(L, -2, "baseSpeed");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(driverPtr, DRIVER_OFFSET_FIRE_SPEED));
    lua_setfield(L, -2, "fireSpeed");

    /* Extended fields */
    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS32(driverPtr, DRIVER_OFFSET_LAP_TIME));
    lua_setfield(L, -2, "lapTime");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadU8(driverPtr, DRIVER_OFFSET_LAP_INDEX));
    lua_setfield(L, -2, "lapIndex");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadU8(driverPtr, DRIVER_OFFSET_NUM_WUMPAS));
    lua_setfield(L, -2, "numWumpas");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(driverPtr, DRIVER_OFFSET_RANK));
    lua_setfield(L, -2, "rank");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS32(driverPtr, DRIVER_OFFSET_POS_CURR));
    lua_setfield(L, -2, "posX");
    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS32(driverPtr, DRIVER_OFFSET_POS_CURR + 4));
    lua_setfield(L, -2, "posY");
    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS32(driverPtr, DRIVER_OFFSET_POS_CURR + 8));
    lua_setfield(L, -2, "posZ");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS32(driverPtr, DRIVER_OFFSET_VELOCITY));
    lua_setfield(L, -2, "velX");
    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS32(driverPtr, DRIVER_OFFSET_VELOCITY + 4));
    lua_setfield(L, -2, "velY");
    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS32(driverPtr, DRIVER_OFFSET_VELOCITY + 8));
    lua_setfield(L, -2, "velZ");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(driverPtr, DRIVER_OFFSET_CONST_GRAVITY));
    lua_setfield(L, -2, "const_Gravity");

    return 1;
}

/* mod.getNumPlayers() — Returns the number of players in the current game */
static int NativeMods_Lua_GetNumPlayers(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)s_mods.cachedNumPlayers);
    return 1;
}

/* mod.getGameMode() — Returns the current gameMode1 value */
static int NativeMods_Lua_GetGameMode(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)s_mods.cachedGameMode1);
    return 1;
}

/* mod.readS16(pointer, offset) — Read a signed 16-bit value
 * pointer is a lightuserdata, offset is an integer
 */
static int NativeMods_Lua_ReadS16(lua_State *L)
{
    if (!lua_islightuserdata(L, 1))
        return luaL_argerror(L, 1, "expected lightuserdata (pointer)");
    void *ptr = lua_touserdata(L, 1);
    int offset = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(ptr, offset));
    return 1;
}

/* mod.readU8(pointer, offset) — Read an unsigned 8-bit value */
static int NativeMods_Lua_ReadU8(lua_State *L)
{
    if (!lua_islightuserdata(L, 1))
        return luaL_argerror(L, 1, "expected lightuserdata (pointer)");
    void *ptr = lua_touserdata(L, 1);
    int offset = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, (lua_Integer)NativeMods_ReadU8(ptr, offset));
    return 1;
}

/* mod.readS32(pointer, offset) — Read a signed 32-bit value */
static int NativeMods_Lua_ReadS32(lua_State *L)
{
    if (!lua_islightuserdata(L, 1))
        return luaL_argerror(L, 1, "expected lightuserdata (pointer)");
    void *ptr = lua_touserdata(L, 1);
    int offset = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS32(ptr, offset));
    return 1;
}

/* ============================================================
 * Write API — allow mods to modify game state
 * ============================================================ */

/* mod.writeS16(pointer, offset, value) — Write a signed 16-bit value */
static int NativeMods_Lua_WriteS16(lua_State *L)
{
    if (!lua_islightuserdata(L, 1))
        return luaL_argerror(L, 1, "expected lightuserdata (pointer)");
    void *ptr = lua_touserdata(L, 1);
    int offset = (int)luaL_checkinteger(L, 2);
    s16 value  = (s16)luaL_checkinteger(L, 3);
    NativeMods_WriteS16(ptr, offset, value);
    return 0;
}

/* mod.writeU8(pointer, offset, value) — Write an unsigned 8-bit value */
static int NativeMods_Lua_WriteU8(lua_State *L)
{
    if (!lua_islightuserdata(L, 1))
        return luaL_argerror(L, 1, "expected lightuserdata (pointer)");
    void *ptr = lua_touserdata(L, 1);
    int offset = (int)luaL_checkinteger(L, 2);
    u8 value   = (u8)luaL_checkinteger(L, 3);
    NativeMods_WriteU8(ptr, offset, value);
    return 0;
}

/* mod.writeS32(pointer, offset, value) — Write a signed 32-bit value */
static int NativeMods_Lua_WriteS32(lua_State *L)
{
    if (!lua_islightuserdata(L, 1))
        return luaL_argerror(L, 1, "expected lightuserdata (pointer)");
    void *ptr = lua_touserdata(L, 1);
    int offset = (int)luaL_checkinteger(L, 2);
    s32 value  = (s32)luaL_checkinteger(L, 3);
    NativeMods_WriteS32(ptr, offset, value);
    return 0;
}

/* ============================================================
 * Driver write API — safe, named-field write access
 * ============================================================ */

/* Field descriptor for driver set operations */
struct DriverFieldDesc
{
    const char *name;
    int offset;
    int size; /* 1=u8, 2=s16, 4=s32 */
};

/* Table of writable driver fields.
 * Only fields that make sense to modify are listed. */
static const struct DriverFieldDesc s_writableDriverFields[] =
{
    {"reserves",                 DRIVER_OFFSET_RESERVES,              2},
    {"fireSpeedCap",             DRIVER_OFFSET_FIRE_SPEED_CAP,        2},
    {"turbo_MeterRoomLeft",      DRIVER_OFFSET_TURBO_METER_ROOM_LEFT, 2},
    {"turbo_outsideTimer",       DRIVER_OFFSET_TURBO_OUTSIDE_TIMER,   2},
    {"numTurbos",                DRIVER_OFFSET_NUM_TURBOS,            1},
    {"kartState",                DRIVER_OFFSET_KART_STATE,            1},
    {"actionsFlagSet",           DRIVER_OFFSET_ACTIONS_FLAG_SET,      2},
    {"speedApprox",              DRIVER_OFFSET_SPEED_APPROX,          2},
    {"const_SacredFireSpeed",    DRIVER_OFFSET_CONST_SACRED_FIRE,     2},
    {"const_SingleTurboSpeed",   DRIVER_OFFSET_CONST_SINGLE_TURBO,    2},
    {"const_turboMaxRoom",       DRIVER_OFFSET_CONST_TURBO_MAX_ROOM,  1},
    {"const_Accel_Reserves",     DRIVER_OFFSET_CONST_ACCEL_RESERVES,  2},
    {"speed",                    DRIVER_OFFSET_SPEED,                  2},
    {"baseSpeed",                DRIVER_OFFSET_BASE_SPEED,             2},
    {"fireSpeed",                DRIVER_OFFSET_FIRE_SPEED,             2},
    {"lapTime",                  DRIVER_OFFSET_LAP_TIME,              4},
    {"lapIndex",                 DRIVER_OFFSET_LAP_INDEX,              1},
    {"numWumpas",                DRIVER_OFFSET_NUM_WUMPAS,            1},
    {"rank",                     DRIVER_OFFSET_RANK,                   2},
    {"const_Gravity",            DRIVER_OFFSET_CONST_GRAVITY,         2},
    { NULL, 0, 0 } /* sentinel */
};

/* mod.setDriverField(playerIndex, fieldName, value) — Write a driver field safely by name
 *
 * playerIndex: 0-7
 * fieldName:   one of "reserves", "fireSpeedCap", "turbo_MeterRoomLeft",
 *              "turbo_outsideTimer", "numTurbos", "kartState",
 *              "actionsFlagSet", "speedApprox"
 * value:       integer value to write
 *
 * Returns true on success, false on failure (invalid index/name/null driver)
 */
static int NativeMods_Lua_SetDriverField(lua_State *L)
{
    int index = (int)luaL_checkinteger(L, 1);
    const char *fieldName = luaL_checkstring(L, 2);
    lua_Integer value = luaL_checkinteger(L, 3);

    if (index < 0 || index > 7)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    void *driverPtr = s_mods.cachedDriverPtrs[index];
    if (!driverPtr)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    /* Find the field descriptor */
    const struct DriverFieldDesc *desc = s_writableDriverFields;
    while (desc->name != NULL)
    {
        if (strcmp(desc->name, fieldName) == 0)
            break;
        desc++;
    }

    if (desc->name == NULL)
    {
        /* Unknown field name */
        char errMsg[128];
        snprintf(errMsg, sizeof(errMsg),
            "unknown driver field '%s' (writable: reserves, fireSpeedCap, turbo_MeterRoomLeft, turbo_outsideTimer, numTurbos, kartState, actionsFlagSet, speedApprox, speed, baseSpeed, fireSpeed, const_SacredFireSpeed, const_SingleTurboSpeed, const_turboMaxRoom, const_Accel_Reserves)",
            fieldName);
        return luaL_argerror(L, 2, errMsg);
    }

    /* Write the value based on field size */
    switch (desc->size)
    {
        case 1: NativeMods_WriteU8(driverPtr, desc->offset, (u8)value);   break;
        case 2: NativeMods_WriteS16(driverPtr, desc->offset, (s16)value);  break;
        case 4: NativeMods_WriteS32(driverPtr, desc->offset, (s32)value);  break;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* ============================================================
 * GameTracker access API
 * ============================================================ */

/* mod.getGameTracker() — Returns a lightuserdata pointer to the GameTracker
 *
 * Combined with readS16/readU8/readS32/writeS16/writeU8/writeS32,
 * this allows full access to the game state beyond the driver struct.
 *
 * Known GameTracker offsets (for advanced modders):
 *   0x000  gameMode1       (s32)
 *   0x343  numPlyrCurrGame (u8)
 *   0x24EC drivers[]       (pointer array, 8 entries)
 *   etc.
 */
static int NativeMods_Lua_GetGameTracker(lua_State *L)
{
    if (!sdata || !sdata->gGT)
    {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlightuserdata(L, (void *)sdata->gGT);
    return 1;
}

/* ============================================================
 * NEW: Drawing API for Lua mods
 *
 * These functions queue drawing commands that are flushed
 * during the ON_RENDER hook via NativeMods_FlushDrawQueue().
 * This avoids Lua→C draw calls from happening at random times.
 * ============================================================ */

#define NATIVE_MODS_DRAW_QUEUE_SIZE 256

enum NativeModDrawCmdType
{
    NATIVE_MOD_DRAW_RECT,
    NATIVE_MOD_DRAW_TEXT
};

struct NativeModDrawCmd
{
    enum NativeModDrawCmdType type;
    union
    {
        struct
        {
            s16 x, y;
            s16 w, h;
            u8 r, g, b, a;
        } rect;
        struct
        {
            char text[128];
            s16 x, y;
            s16 fontType;
            s16 justify;
        } text;
    };
};

static struct NativeModDrawCmd s_drawQueue[NATIVE_MODS_DRAW_QUEUE_SIZE];
static int s_drawQueueCount = 0;

/* mod.drawRect(x, y, w, h, r, g, b, [a]) — Queue a colored rectangle */
static int NativeMods_Lua_DrawRect(lua_State *L)
{
    if (s_drawQueueCount >= NATIVE_MODS_DRAW_QUEUE_SIZE)
    {
        fprintf(stderr, "[Mods] Draw queue overflow, ignoring drawRect\n");
        return 0;
    }

    struct NativeModDrawCmd *cmd = &s_drawQueue[s_drawQueueCount++];
    cmd->type = NATIVE_MOD_DRAW_RECT;
    cmd->rect.x = (s16)luaL_checkinteger(L, 1);
    cmd->rect.y = (s16)luaL_checkinteger(L, 2);
    cmd->rect.w = (s16)luaL_checkinteger(L, 3);
    cmd->rect.h = (s16)luaL_checkinteger(L, 4);
    cmd->rect.r = (u8)luaL_checkinteger(L, 5);
    cmd->rect.g = (u8)luaL_checkinteger(L, 6);
    cmd->rect.b = (u8)luaL_checkinteger(L, 7);
    cmd->rect.a = (u8)luaL_optinteger(L, 8, 255);

    return 0;
}

/* mod.drawText(text, x, y, [fontType], [justify]) — Queue text drawing
 * fontType: 1=big, 2=small (default: 2)
 * justify: 0=left, 1=center, 2=right (default: 0)
 */
static int NativeMods_Lua_DrawText(lua_State *L)
{
    if (s_drawQueueCount >= NATIVE_MODS_DRAW_QUEUE_SIZE)
    {
        fprintf(stderr, "[Mods] Draw queue overflow, ignoring drawText\n");
        return 0;
    }

    const char *text = luaL_checkstring(L, 1);
    if (!text)
        return 0;

    struct NativeModDrawCmd *cmd = &s_drawQueue[s_drawQueueCount++];
    cmd->type = NATIVE_MOD_DRAW_TEXT;
    strncpy(cmd->text.text, text, sizeof(cmd->text.text) - 1);
    cmd->text.text[sizeof(cmd->text.text) - 1] = '\0';
    cmd->text.x = (s16)luaL_checkinteger(L, 2);
    cmd->text.y = (s16)luaL_checkinteger(L, 3);
    cmd->text.fontType = (s16)luaL_optinteger(L, 4, 2);
    cmd->text.justify = (s16)luaL_optinteger(L, 5, 0);

    return 0;
}

/* ============================================================
 * NEW: Gamepad input API for Lua mods
 * ============================================================ */

/* mod.getGamepad(padIndex) — Returns a table with gamepad state for pad 0-7
 *
 * Returned table fields:
 *   held     (int) — buttons held this frame  (bitmask, see mod.BUTTONS)
 *   tapped   (int) — buttons pressed this frame
 *   released (int) — buttons released this frame
 *   prevHeld (int) — buttons held last frame
 *   stickLX  (int) — left stick X  (0=left, 128=center, 255=right)
 *   stickLY  (int) — left stick Y
 *   stickRX  (int) — right stick X
 *   stickRY  (int) — right stick Y  (128=neutral, <128=brake, >128=gas)
 */
static int NativeMods_Lua_GetGamepad(lua_State *L)
{
    int index = (int)luaL_checkinteger(L, 1);
    if (index < 0 || index > 7)
        return luaL_argerror(L, 1, "pad index must be 0-7");

    if (!sdata || !sdata->gGamepads)
    {
        lua_pushnil(L);
        return 1;
    }

    struct GamepadBuffer *pad = &sdata->gGamepads->gamepad[index];

    lua_newtable(L);

    lua_pushinteger(L, (lua_Integer)pad->buttonsHeldCurrFrame);
    lua_setfield(L, -2, "held");

    lua_pushinteger(L, (lua_Integer)pad->buttonsTapped);
    lua_setfield(L, -2, "tapped");

    lua_pushinteger(L, (lua_Integer)pad->buttonsReleased);
    lua_setfield(L, -2, "released");

    lua_pushinteger(L, (lua_Integer)pad->buttonsHeldPrevFrame);
    lua_setfield(L, -2, "prevHeld");

    lua_pushinteger(L, (lua_Integer)pad->stickLX);
    lua_setfield(L, -2, "stickLX");

    lua_pushinteger(L, (lua_Integer)pad->stickLY);
    lua_setfield(L, -2, "stickLY");

    lua_pushinteger(L, (lua_Integer)pad->stickRX);
    lua_setfield(L, -2, "stickRX");

    lua_pushinteger(L, (lua_Integer)pad->stickRY);
    lua_setfield(L, -2, "stickRY");

    return 1;
}

/* mod.getGamepads() — Returns a lightuserdata to the GamepadSystem struct
 *
 * Combined with readS16/readU8/readS32, allows raw access to all gamepad data.
 * Each gamepad buffer is 0x50 bytes.
 * GamepadBuffer field offsets (from base):
 *   0x04  stickLX       (s16)
 *   0x06  stickLY       (s16)
 *   0x0C  stickRX       (s16)
 *   0x0E  stickRY       (s16)
 *   0x10  buttonsHeldCurrFrame (s32)
 *   0x14  buttonsTapped        (s32)
 *   0x18  buttonsReleased      (s32)
 *   0x1C  buttonsHeldPrevFrame (s32)
 */
static int NativeMods_Lua_GetGamepads(lua_State *L)
{
    if (!sdata || !sdata->gGamepads)
    {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlightuserdata(L, (void *)sdata->gGamepads);
    return 1;
}

/* ============================================================
 * NEW: Instance API — read/modify game Instance structs
 * ============================================================ */

/* Instance struct field offsets (from namespace_Instance.h) */
#define INSTANCE_OFFSET_NAME        0x08
#define INSTANCE_OFFSET_SCALE       0x18  /* s16 scale[3] */
#define INSTANCE_OFFSET_ALPHA_SCALE 0x22  /* s16 */
#define INSTANCE_OFFSET_COLOR_RGBA  0x24  /* u32 */
#define INSTANCE_OFFSET_FLAGS       0x28  /* u32 */
#define INSTANCE_OFFSET_ANIM_INDEX  0x52  /* u8 */
#define INSTANCE_OFFSET_ANIM_FRAME  0x54  /* s16 */
/* MATRIX is at 0x30, translation vector at offset 18 bytes into MATRIX */
#define INSTANCE_OFFSET_POS_X       0x42  /* s16 */
#define INSTANCE_OFFSET_POS_Y       0x44  /* s16 */
#define INSTANCE_OFFSET_POS_Z       0x46  /* s16 */

/* Helper: push an instance table for the given pointer */
static void NativeMods_PushInstanceTable(lua_State *L, void *ptr)
{
    if (!ptr)
    {
        lua_pushnil(L);
        return;
    }

    lua_newtable(L);

    char name[16];
    memcpy(name, (char *)ptr + INSTANCE_OFFSET_NAME, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    lua_pushstring(L, name);
    lua_setfield(L, -2, "name");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS32(ptr, INSTANCE_OFFSET_COLOR_RGBA));
    lua_setfield(L, -2, "colorRGBA");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(ptr, INSTANCE_OFFSET_ALPHA_SCALE));
    lua_setfield(L, -2, "alphaScale");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS32(ptr, INSTANCE_OFFSET_FLAGS));
    lua_setfield(L, -2, "flags");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(ptr, INSTANCE_OFFSET_ANIM_FRAME));
    lua_setfield(L, -2, "animFrame");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadU8(ptr, INSTANCE_OFFSET_ANIM_INDEX));
    lua_setfield(L, -2, "animIndex");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(ptr, INSTANCE_OFFSET_POS_X));
    lua_setfield(L, -2, "posX");
    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(ptr, INSTANCE_OFFSET_POS_Y));
    lua_setfield(L, -2, "posY");
    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(ptr, INSTANCE_OFFSET_POS_Z));
    lua_setfield(L, -2, "posZ");

    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(ptr, INSTANCE_OFFSET_SCALE));
    lua_setfield(L, -2, "scaleX");
    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(ptr, INSTANCE_OFFSET_SCALE + 2));
    lua_setfield(L, -2, "scaleY");
    lua_pushinteger(L, (lua_Integer)NativeMods_ReadS16(ptr, INSTANCE_OFFSET_SCALE + 4));
    lua_setfield(L, -2, "scaleZ");

    lua_pushlightuserdata(L, ptr);
    lua_setfield(L, -2, "ptr");
}

/* Instance writable field table */
struct InstanceFieldDesc
{
    const char *name;
    int offset;
    int size; /* 1=u8, 2=s16, 4=s32 */
};

static const struct InstanceFieldDesc s_writableInstanceFields[] =
{
    {"colorRGBA",  INSTANCE_OFFSET_COLOR_RGBA,  4},
    {"alphaScale", INSTANCE_OFFSET_ALPHA_SCALE,  2},
    {"flags",      INSTANCE_OFFSET_FLAGS,        4},
    {"animFrame",  INSTANCE_OFFSET_ANIM_FRAME,   2},
    {"animIndex",  INSTANCE_OFFSET_ANIM_INDEX,   1},
    {"scaleX",     INSTANCE_OFFSET_SCALE,        2},
    {"scaleY",     INSTANCE_OFFSET_SCALE + 2,    2},
    {"scaleZ",     INSTANCE_OFFSET_SCALE + 4,    2},
    { NULL, 0, 0 }
};

/* mod.getInstance(ptr) — Returns a table with Instance fields */
static int NativeMods_Lua_GetInstance(lua_State *L)
{
    if (!lua_islightuserdata(L, 1))
        return luaL_argerror(L, 1, "expected lightuserdata (pointer)");
    NativeMods_PushInstanceTable(L, lua_touserdata(L, 1));
    return 1;
}

/* mod.forEachInstance(callback) — Iterates all active instances */
static int NativeMods_Lua_ForEachInstance(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);

    if (!sdata || !sdata->gGT)
    {
        lua_pushinteger(L, 0);
        return 1;
    }

    struct Instance *inst = (struct Instance *)sdata->gGT->JitPools.instance.taken.first;
    int count = 0;

    while (inst != NULL)
    {
        lua_pushvalue(L, 1);
        NativeMods_PushInstanceTable(L, inst);
        if (lua_pcall(L, 1, 1, 0) != LUA_OK)
        {
            NativeMods_ReportLuaError(L, "forEachInstance");
            break;
        }
        if (!lua_toboolean(L, -1))
        {
            lua_pop(L, 1);
            break;
        }
        lua_pop(L, 1);
        inst = inst->next;
        count++;
    }

    lua_pushinteger(L, count);
    return 1;
}

/* mod.findInstancesByName(name) — Returns array of instances matching name */
static int NativeMods_Lua_FindInstancesByName(lua_State *L)
{
    const char *searchName = luaL_checkstring(L, 1);
    if (!searchName)
    {
        lua_pushnil(L);
        return 1;
    }

    if (!sdata || !sdata->gGT)
    {
        lua_newtable(L);
        return 1;
    }

    lua_newtable(L);
    int index = 1;
    struct Instance *inst = (struct Instance *)sdata->gGT->JitPools.instance.taken.first;

    while (inst != NULL)
    {
        char name[16];
        memcpy(name, (char *)inst + INSTANCE_OFFSET_NAME, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';

        if (strcmp(name, searchName) == 0)
        {
            NativeMods_PushInstanceTable(L, inst);
            lua_rawseti(L, -2, index++);
        }
        inst = inst->next;
    }

    return 1;
}

/* mod.setInstanceField(ptr, fieldName, value) — Write an instance field by name */
static int NativeMods_Lua_SetInstanceField(lua_State *L)
{
    if (!lua_islightuserdata(L, 1))
        return luaL_argerror(L, 1, "expected lightuserdata (pointer)");
    void *ptr = lua_touserdata(L, 1);
    if (!ptr)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    const char *fieldName = luaL_checkstring(L, 2);
    lua_Integer value = luaL_checkinteger(L, 3);

    const struct InstanceFieldDesc *desc = s_writableInstanceFields;
    while (desc->name != NULL)
    {
        if (strcmp(desc->name, fieldName) == 0)
            break;
        desc++;
    }

    if (desc->name == NULL)
    {
        return luaL_argerror(L, 2,
            "unknown instance field (writable: colorRGBA, alphaScale, flags, animFrame, animIndex, scaleX, scaleY, scaleZ)");
    }

    switch (desc->size)
    {
        case 1: NativeMods_WriteU8(ptr, desc->offset, (u8)value);   break;
        case 2: NativeMods_WriteS16(ptr, desc->offset, (s16)value);  break;
        case 4: NativeMods_WriteS32(ptr, desc->offset, (s32)value);  break;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* ============================================================
 * NEW: Track name API
 * ============================================================ */

/* mod.getTrackName() — Returns the current track/level name string */
static int NativeMods_Lua_GetTrackName(lua_State *L)
{
    if (!sdata || !sdata->gGT)
    {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, sdata->gGT->levelName);
    return 1;
}

/* ============================================================
 * NEW: Extended drawing API — circle and line primitives
 * ============================================================ */

/* mod.drawCircle(cx, cy, radius, r, g, b, [a]) — Queue a filled circle */
static int NativeMods_Lua_DrawCircle(lua_State *L)
{
    if (s_drawQueueCount >= NATIVE_MODS_DRAW_QUEUE_SIZE)
    {
        fprintf(stderr, "[Mods] Draw queue overflow, ignoring drawCircle\n");
        return 0;
    }

    s16 cx      = (s16)luaL_checkinteger(L, 1);
    s16 cy      = (s16)luaL_checkinteger(L, 2);
    s16 radius  = (s16)luaL_checkinteger(L, 3);
    u8 r        = (u8)luaL_checkinteger(L, 4);
    u8 g        = (u8)luaL_checkinteger(L, 5);
    u8 b        = (u8)luaL_checkinteger(L, 6);
    u8 a        = (u8)luaL_optinteger(L, 7, 255);

    /* Approximate a filled circle using a bounding box rectangle
     * and a semi-transparent overlay. For a proper circle we'd need
     * polygon drawing, but for HUD overlays this approximation works. */
    struct NativeModDrawCmd *cmd = &s_drawQueue[s_drawQueueCount++];
    cmd->type = NATIVE_MOD_DRAW_RECT;
    cmd->rect.x = cx - radius;
    cmd->rect.y = cy - radius;
    cmd->rect.w = radius * 2;
    cmd->rect.h = radius * 2;
    cmd->rect.r = r;
    cmd->rect.g = g;
    cmd->rect.b = b;
    cmd->rect.a = a;

    return 0;
}

/* mod.drawLine(x1, y1, x2, y2, r, g, b, [a]) — Queue a line */
static int NativeMods_Lua_DrawLine(lua_State *L)
{
    if (s_drawQueueCount >= NATIVE_MODS_DRAW_QUEUE_SIZE)
    {
        fprintf(stderr, "[Mods] Draw queue overflow, ignoring drawLine\n");
        return 0;
    }

    s16 x1 = (s16)luaL_checkinteger(L, 1);
    s16 y1 = (s16)luaL_checkinteger(L, 2);
    s16 x2 = (s16)luaL_checkinteger(L, 3);
    s16 y2 = (s16)luaL_checkinteger(L, 4);
    u8 r   = (u8)luaL_checkinteger(L, 5);
    u8 g   = (u8)luaL_checkinteger(L, 6);
    u8 b   = (u8)luaL_checkinteger(L, 7);
    u8 a   = (u8)luaL_optinteger(L, 8, 255);

    /* Draw line as a thin rect between the two points.
     * For a proper line we'd need LINE primitive, but this works
     * for axis-aligned HUD elements. */
    s16 x = x1 < x2 ? x1 : x2;
    s16 y = y1 < y2 ? y1 : y2;
    s16 w = x1 < x2 ? (x2 - x1) : (x1 - x2);
    s16 h = y1 < y2 ? (y2 - y1) : (y1 - y2);
    if (w < 2) w = 2;   /* ensure visibility */
    if (h < 2) h = 2;

    struct NativeModDrawCmd *cmd = &s_drawQueue[s_drawQueueCount++];
    cmd->type = NATIVE_MOD_DRAW_RECT;
    cmd->rect.x = x;
    cmd->rect.y = y;
    cmd->rect.w = w;
    cmd->rect.h = h;
    cmd->rect.r = r;
    cmd->rect.g = g;
    cmd->rect.b = b;
    cmd->rect.a = a;

    return 0;
}

/* Forward declarations for functions defined later */
static int NativeMods_Lua_GetHookContext(lua_State *L);

/* ============================================================
 * Lua library registration table
 * ============================================================ */

static const struct luaL_Reg s_nativeModLib[] = {
    /* Original API */
    {"log", NativeMods_Lua_Log},
    {"getModPath", NativeMods_Lua_GetModPath},
    {"readFile", NativeMods_Lua_ReadFile},
    {"writeFile", NativeMods_Lua_WriteFile},
    {"hook", NativeMods_Lua_Hook},

    /* Game memory access (read) */
    {"getDriver", NativeMods_Lua_GetDriver},
    {"getNumPlayers", NativeMods_Lua_GetNumPlayers},
    {"getGameMode", NativeMods_Lua_GetGameMode},
    {"getGameTracker", NativeMods_Lua_GetGameTracker},
    {"readS16", NativeMods_Lua_ReadS16},
    {"readU8", NativeMods_Lua_ReadU8},
    {"readS32", NativeMods_Lua_ReadS32},

    /* Game memory access (write) */
    {"writeS16", NativeMods_Lua_WriteS16},
    {"writeU8", NativeMods_Lua_WriteU8},
    {"writeS32", NativeMods_Lua_WriteS32},
    {"setDriverField", NativeMods_Lua_SetDriverField},

    /* Drawing */
    {"drawRect", NativeMods_Lua_DrawRect},
    {"drawText", NativeMods_Lua_DrawText},

    /* Gamepad input */
    {"getGamepad", NativeMods_Lua_GetGamepad},
    {"getGamepads", NativeMods_Lua_GetGamepads},

    /* Hook context */
    {"getHookContext", NativeMods_Lua_GetHookContext},

    /* Instance API */
    {"getInstance", NativeMods_Lua_GetInstance},
    {"forEachInstance", NativeMods_Lua_ForEachInstance},
    {"findInstancesByName", NativeMods_Lua_FindInstancesByName},
    {"setInstanceField", NativeMods_Lua_SetInstanceField},

    /* Extended drawing */
    {"drawCircle", NativeMods_Lua_DrawCircle},
    {"drawLine", NativeMods_Lua_DrawLine},

    /* Track info */
    {"getTrackName", NativeMods_Lua_GetTrackName},

    {NULL, NULL}
};

/* ============================================================
 * Initialization and mod scanning
 * ============================================================ */

void NativeMods_Shutdown(void);

int NativeMods_Init(void)
{
    if (s_mods.initialized)
        return 1;

    memset(&s_mods, 0, sizeof(s_mods));

    s_mods.L = luaL_newstate();
    if (!s_mods.L)
    {
        fprintf(stderr, "[Mods] Failed to create Lua state.\n");
        return 0;
    }

    lua_atpanic(s_mods.L, NativeMods_LuaPanic);
    luaL_openlibs(s_mods.L);

    lua_newtable(s_mods.L);
    luaL_setfuncs(s_mods.L, s_nativeModLib, 0);

    /* Add mod.BUTTONS constant table */
    lua_newtable(s_mods.L);
    lua_pushinteger(s_mods.L, 0x00001); lua_setfield(s_mods.L, -2, "UP");
    lua_pushinteger(s_mods.L, 0x00002); lua_setfield(s_mods.L, -2, "DOWN");
    lua_pushinteger(s_mods.L, 0x00004); lua_setfield(s_mods.L, -2, "LEFT");
    lua_pushinteger(s_mods.L, 0x00008); lua_setfield(s_mods.L, -2, "RIGHT");
    lua_pushinteger(s_mods.L, 0x04010); lua_setfield(s_mods.L, -2, "CROSS");
    lua_pushinteger(s_mods.L, 0x08020); lua_setfield(s_mods.L, -2, "SQUARE");
    lua_pushinteger(s_mods.L, 0x00040); lua_setfield(s_mods.L, -2, "CIRCLE");
    lua_pushinteger(s_mods.L, 0x40000); lua_setfield(s_mods.L, -2, "TRIANGLE");
    lua_pushinteger(s_mods.L, 0x00800); lua_setfield(s_mods.L, -2, "L1");
    lua_pushinteger(s_mods.L, 0x00400); lua_setfield(s_mods.L, -2, "R1");
    lua_pushinteger(s_mods.L, 0x00180); lua_setfield(s_mods.L, -2, "L2");
    lua_pushinteger(s_mods.L, 0x00200); lua_setfield(s_mods.L, -2, "R2");
    lua_pushinteger(s_mods.L, 0x01000); lua_setfield(s_mods.L, -2, "START");
    lua_pushinteger(s_mods.L, 0x02000); lua_setfield(s_mods.L, -2, "SELECT");
    lua_pushinteger(s_mods.L, 0x10000); lua_setfield(s_mods.L, -2, "L3");
    lua_pushinteger(s_mods.L, 0x20000); lua_setfield(s_mods.L, -2, "R3");
    lua_setfield(s_mods.L, -2, "BUTTONS");

    lua_setglobal(s_mods.L, "mod");

    for (int i = 0; i < NATIVE_MOD_HOOK_COUNT; i++)
        s_mods.luaRefs[i] = LUA_NOREF;

    s_mods.initialized = 1;
    fprintf(stdout, "[Mods] Lua VM initialized (with draw + memory API).\n");

    /* Save mod state on exit so enabled/disabled survives restart. */
    atexit(NativeMods_Shutdown);

    return 1;
}

/* ============================================================
 * Persist mod enabled/disabled state across sessions
 * ============================================================ */

static void NativeMods_SaveState(void)
{
    char statePath[NATIVE_MODS_PATH_MAX];
    if (!NativeAssets_BuildPathFromBase(NATIVE_MODS_STATE_FILE, statePath, sizeof(statePath)))
        return;

    FILE *f = fopen(statePath, "w");
    if (!f)
        return;

    for (int i = 0; i < s_mods.modCount; i++)
        fprintf(f, "%s=%d\n", s_mods.mods[i].name, s_mods.mods[i].enabled ? 1 : 0);

    fclose(f);
}

static void NativeMods_LoadState(void)
{
    char statePath[NATIVE_MODS_PATH_MAX];
    if (!NativeAssets_BuildPathFromBase(NATIVE_MODS_STATE_FILE, statePath, sizeof(statePath)))
        return;

    FILE *f = fopen(statePath, "r");
    if (!f)
        return;

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *name = line;
        int enabled = atoi(eq + 1);

        for (int i = 0; i < s_mods.modCount; i++)
        {
            if (strcmp(s_mods.mods[i].name, name) == 0)
            {
                s_mods.mods[i].enabled = enabled ? 1 : 0;
                break;
            }
        }
    }

    fclose(f);
}

void NativeMods_Shutdown(void)
{
    if (!s_mods.initialized)
        return;
    NativeMods_SaveState();
    if (s_mods.L)
    {
        lua_close(s_mods.L);
        s_mods.L = NULL;
    }
    s_mods.initialized = 0;
    s_mods.modCount = 0;
}

int NativeMods_ScanMods(void)
{
    char modsDir[NATIVE_MODS_PATH_MAX];
    DIR *dir;
    struct dirent *entry;

    s_mods.modCount = 0;

    if (!NativeAssets_BuildPathFromBase(NATIVE_MODS_DIR_NAME, modsDir, sizeof(modsDir)))
        return 0;

    dir = opendir(modsDir);
    if (!dir)
    {
        #if defined(_WIN32)
        mkdir(modsDir);
        #else
        mkdir(modsDir, 0755);
        #endif
        return 0;
    }

    while ((entry = readdir(dir)) != NULL && s_mods.modCount < NATIVE_MODS_MAX_MODS)
    {
        char scriptPath[NATIVE_MODS_PATH_MAX];
        struct stat st;
        int idx = s_mods.modCount;

        if (entry->d_name[0] == '.')
            continue;

        int written = snprintf(scriptPath, sizeof(scriptPath), "%s/%s/%s", modsDir, entry->d_name, NATIVE_MODS_SCRIPT_NAME);
        if ((written <= 0) || ((size_t)written >= sizeof(scriptPath)))
            continue;

        if (stat(scriptPath, &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        strncpy(s_mods.mods[idx].name, entry->d_name, sizeof(s_mods.mods[idx].name) - 1);

        written = snprintf(s_mods.mods[idx].path, sizeof(s_mods.mods[idx].path), "%s/%s", modsDir, entry->d_name);
        if ((written <= 0) || ((size_t)written >= sizeof(s_mods.mods[idx].path)))
            continue;

        s_mods.mods[idx].enabled = 1;
        s_mods.modCount++;
        fprintf(stdout, "[Mods] Found mod: %s\n", entry->d_name);
    }

    closedir(dir);

    /* Restore previously saved enabled/disabled states */
    NativeMods_LoadState();

    return s_mods.modCount;
}

int NativeMods_GetModCount(void)
{
    return s_mods.modCount;
}

const struct NativeModInfo *NativeMods_GetMod(int index)
{
    if (index < 0 || index >= s_mods.modCount)
        return NULL;
    return &s_mods.mods[index];
}

int NativeMods_ToggleMod(int index)
{
    if (index < 0 || index >= s_mods.modCount)
        return 0;
    s_mods.mods[index].enabled = !s_mods.mods[index].enabled;

    /* Clear all Lua hook refs so disabled mods stop firing. */
    for (int i = 0; i < NATIVE_MOD_HOOK_COUNT; i++)
        s_mods.luaRefs[i] = LUA_NOREF;

    /* Reload all enabled mod scripts so hooks reflect the new state. */
    NativeMods_LoadModScripts();

    /* Reload the language file from disk so that any mod file
     * override for the language file is applied or reverted.
     * LOAD_LangFile is declared in functions.h (included via
     * common.h before this file in the unity build). */
    if (sdata != NULL && sdata->ptrBigfile1 != NULL)
    {
        LOAD_LangFile((int)sdata->ptrBigfile1, 1);
        NativeMods_OnLanguageLoaded(sdata->lngStrings, sdata->numLngStrings);
    }

    /* Persist the new state so it survives game restarts. */
    NativeMods_SaveState();

    return 1;
}

int NativeMods_IsModEnabled(int index)
{
    if (index < 0 || index >= s_mods.modCount)
        return 0;
    return s_mods.mods[index].enabled;
}

const char *NativeMods_GetModsDir(void)
{
    return NATIVE_MODS_DIR_NAME;
}

int NativeMods_LoadModScripts(void)
{
    if (!s_mods.initialized)
        return 0;

    for (int i = 0; i < s_mods.modCount; i++)
    {
        if (!s_mods.mods[i].enabled)
            continue;

        char scriptPath[NATIVE_MODS_PATH_MAX];

        int written = snprintf(scriptPath, sizeof(scriptPath), "%s/%s", s_mods.mods[i].path, NATIVE_MODS_SCRIPT_NAME);
        if ((written <= 0) || ((size_t)written >= sizeof(scriptPath)))
            continue;

        strncpy(s_mods.activeModName, s_mods.mods[i].name, sizeof(s_mods.activeModName) - 1);
        strncpy(s_mods.activeModPath, s_mods.mods[i].path, sizeof(s_mods.activeModPath) - 1);

        if (luaL_dofile(s_mods.L, scriptPath) != LUA_OK)
        {
            NativeMods_ReportLuaError(s_mods.L, s_mods.mods[i].name);
            continue;
        }

        fprintf(stdout, "[Mods] Loaded: %s\n", s_mods.mods[i].name);
    }

    s_mods.activeModName[0] = '\0';
    s_mods.activeModPath[0] = '\0';

    NativeMods_CallHook(NATIVE_MOD_HOOK_ON_INIT);
    return 1;
}

/* ============================================================
 * Hook dispatch
 * ============================================================ */

void NativeMods_CallHook(enum NativeModHookType hook)
{
    if (!s_mods.initialized || hook < 0 || hook >= NATIVE_MOD_HOOK_COUNT)
        return;

    if (s_mods.luaRefs[hook] == LUA_NOREF)
        return;

    lua_rawgeti(s_mods.L, LUA_REGISTRYINDEX, s_mods.luaRefs[hook]);

    if (lua_pcall(s_mods.L, 0, 0, 0) != LUA_OK)
        NativeMods_ReportLuaError(s_mods.L, "hook callback");
}

void NativeMods_CallHookWithDelta(enum NativeModHookType hook, int dt)
{
    if (!s_mods.initialized || hook < 0 || hook >= NATIVE_MOD_HOOK_COUNT)
        return;

    if (s_mods.luaRefs[hook] == LUA_NOREF)
        return;

    lua_rawgeti(s_mods.L, LUA_REGISTRYINDEX, s_mods.luaRefs[hook]);
    lua_pushinteger(s_mods.L, (lua_Integer)dt);

    if (lua_pcall(s_mods.L, 1, 0, 0) != LUA_OK)
        NativeMods_ReportLuaError(s_mods.L, "hook callback");
}

/* ============================================================
 * Hook context — set from game code before calling CallHook,
 * retrieved from Lua via mod.getHookContext()
 * ============================================================ */

void NativeMods_SetHookContext(int driverIndex, int arg0, int arg1, int arg2, int arg3)
{
    if (!s_mods.initialized)
        return;
    s_mods.hookContext.driverIndex = driverIndex;
    s_mods.hookContext.intArgs[0] = arg0;
    s_mods.hookContext.intArgs[1] = arg1;
    s_mods.hookContext.intArgs[2] = arg2;
    s_mods.hookContext.intArgs[3] = arg3;
}

int NativeMods_FindDriverIndex(void *driver)
{
    for (int i = 0; i < 8; i++)
        if (s_mods.cachedDriverPtrs[i] == driver)
            return i;
    return -1;
}

/* mod.getHookContext() — Returns a table with the current hook context:
 *   { driverIndex = int, args = { int, int, int, int } }
 */
static int NativeMods_Lua_GetHookContext(lua_State *L)
{
    lua_newtable(L);

    lua_pushinteger(L, (lua_Integer)s_mods.hookContext.driverIndex);
    lua_setfield(L, -2, "driverIndex");

    lua_newtable(L);
    for (int i = 0; i < 4; i++)
    {
        lua_pushinteger(L, (lua_Integer)s_mods.hookContext.intArgs[i]);
        lua_rawseti(L, -2, i);
    }
    lua_setfield(L, -2, "args");

    return 1;
}

/* ============================================================
 * Game state cache — called from the game loop to snapshot
 * driver pointers before Lua hooks run
 * ============================================================ */

void NativeMods_CacheGameState(void)
{
    if (!s_mods.initialized || !sdata)
        return;

    struct GameTracker *gGT = sdata->gGT;
    if (!gGT)
        return;

    /* Cache driver pointers */
    for (int i = 0; i < 8; i++)
    {
        s_mods.cachedDriverPtrs[i] = (void *)gGT->drivers[i];
    }

    s_mods.cachedNumPlayers = gGT->numPlyrCurrGame;
    s_mods.cachedGameMode1 = gGT->gameMode1;
}

/* ============================================================
 * Draw queue flush — called from CTR's render path to execute
 * all queued drawing commands using CTR's native primitives
 * ============================================================ */

void NativeMods_FlushDrawQueue(void)
{
    if (s_drawQueueCount == 0)
        return;
    if (!sdata)
        return;

    struct GameTracker *gGT = sdata->gGT;
    if (!gGT)
        return;

    /* Use the UI ordering table for drawing */
    u_long *ot = gGT->pushBuffer_UI.ptrOT;
    if (!ot)
    {
        s_drawQueueCount = 0;
        return;
    }

    for (int i = 0; i < s_drawQueueCount; i++)
    {
        struct NativeModDrawCmd *cmd = &s_drawQueue[i];

        if (cmd->type == NATIVE_MOD_DRAW_RECT)
        {
            s16 x = cmd->rect.x;
            s16 y = cmd->rect.y;
            s16 w = cmd->rect.w;
            s16 h = cmd->rect.h;

            /* Build a ColorCode for the rectangle */
            /* For opaque: use CTR_Box_DrawSolidBox which handles PrimMem allocation */
            /* For semi-transparent: use CTR_Box_DrawClearBox */
            const PrimCode primCode = {.poly = {.quad = 1, .renderCode = RenderCode_Polygon}};
            Color rectColor = MakeColorCode(cmd->rect.r, cmd->rect.g, cmd->rect.b, primCode);

            RECT box;
            box.x = x;
            box.y = y;
            box.w = w;
            box.h = h;

            if (cmd->rect.a >= 255)
            {
                /* Opaque rectangle */
                CTR_Box_DrawSolidBox(&box, rectColor, ot);
            }
            else
            {
                /* Semi-transparent rectangle using CTR_Box_DrawClearBox
                 * transparency parameter: 0=50%, 1=100%+1, 2=100%+2, 3=100%-1
                 * Using 0 for standard 50% semi-transparency */
                CTR_Box_DrawClearBox(&box, &rectColor, 0, ot);
            }
        }
        else if (cmd->type == NATIVE_MOD_DRAW_TEXT)
        {
            s16 x = cmd->text.x;
            s16 y = cmd->text.y;
            s16 fontType = cmd->text.fontType;
            s16 justify = cmd->text.justify;

            /* Map font types:
             * fontType 1 = FONT_BIG (1) — large font
             * fontType 2 = FONT_SMALL (2) — small font
             */
            if (fontType < 1) fontType = 1;
            if (fontType > 2) fontType = 2;

            /* Map justify flags to CTR's JUSTIFY_ constants */
            int flags = 0;
            if (justify == 1) flags = JUSTIFY_CENTER;
            else if (justify == 2) flags = JUSTIFY_RIGHT;

            DecalFont_DrawLineOT(cmd->text.text, x, y, fontType, flags, ot);
        }
    }

    s_drawQueueCount = 0;
}

/* ============================================================
 * Language and file overrides
 * ============================================================ */

void NativeMods_OnLanguageLoaded(char **lngStrings, int numStrings)
{
    static const char modsText[] = "MODS";
    static const char teamRaceText[] = "TEAM RACE";

    if (lngStrings == NULL)
        return;

    if (numStrings > 0x014)
        lngStrings[0x014] = (char *)modsText;

    if (numStrings > 0x0E4)
        lngStrings[0x0E4] = (char *)teamRaceText;
}

FILE *NativeMods_OpenFile(const char *relativePath, const char *mode)
{
    char path[NATIVE_MODS_PATH_MAX];
    FILE *file;

    /* Priority 1: Check each enabled mod's "files/" directory.
     * This allows mods to provide complete file overrides.
     * A mod can replace ANY game asset by placing it in:
     *   mods/my_mod/files/<relative_path>
     * For example, to override a BIGFILE asset:
     *   mods/my_mod/files/BIGFILE/levels/tracks/coco/1P/data.lev
     * Or to provide a custom language file:
     *   mods/my_mod/files/BIGFILE/lang/en.lng
     */
    for (int i = 0; i < s_mods.modCount; i++)
    {
        if (!s_mods.mods[i].enabled)
            continue;

        int written = snprintf(path, sizeof(path), "%s/%s/%s", s_mods.mods[i].path, NATIVE_MODS_FILES_DIR, relativePath);
        if ((written <= 0) || ((size_t)written >= sizeof(path)))
            continue;

        file = fopen(path, mode);
        if (file)
        {
            fprintf(stdout, "[Mods] File override: %s (from mod: %s)\n", relativePath, s_mods.mods[i].name);
            return file;
        }
    }

    /* Priority 2: Check the global BIGFILE/ unpacked folder.
     * This is the shared unpacked asset directory that any mod
     * can add files to (the HYBRID/UNPACKED bigfile path).
     */
    char bigfilePath[NATIVE_MODS_PATH_MAX];
    if (NativeAssets_BuildPath(NATIVE_MODS_BIGFILE_DIR, bigfilePath, sizeof(bigfilePath)))
    {
        char unpackedPath[NATIVE_MODS_PATH_MAX];
        int written = snprintf(unpackedPath, sizeof(unpackedPath), "%s/%s", bigfilePath, relativePath);
        if ((written > 0) && ((size_t)written < sizeof(unpackedPath)))
        {
            file = fopen(unpackedPath, mode);
            if (file)
                return file;
        }
    }

    /* Priority 3: Try the base assets directory directly.
     * This catches assets that are in assets/ but not in BIGFILE/.
     */
    {
        char assetPath[NATIVE_MODS_PATH_MAX];
        if (NativeAssets_BuildPath(relativePath, assetPath, sizeof(assetPath)))
        {
            file = fopen(assetPath, mode);
            if (file)
                return file;
        }
    }

    return NULL;
}
