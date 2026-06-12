#include <macros.h>
#include <platform/native_mods.h>
#include <platform/native_assets.h>
#include <psx/types.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define NATIVE_MODS_DIR_NAME         "mods"
#define NATIVE_MODS_SCRIPT_NAME      "main.lua"
#define NATIVE_MODS_FILES_DIR        "files"
#define NATIVE_MODS_BIGFILE_DIR      "BIGFILE"

struct NativeModsState
{
    lua_State *L;
    struct NativeModInfo mods[NATIVE_MODS_MAX_MODS];
    int modCount;
    int initialized;
    int luaRefs[NATIVE_MOD_HOOK_COUNT];
    char activeModName[NATIVE_MODS_NAME_MAX];
    char activeModPath[NATIVE_MODS_PATH_MAX];
};

global_variable struct NativeModsState s_mods;

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
    else
        return luaL_argerror(L, 1, "unknown hook name (onInit, onUpdate, onRender, onInput, onTitleInit)");

    lua_pushvalue(L, 2);
    s_mods.luaRefs[hookType] = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

static const struct luaL_Reg s_nativeModLib[] = {
    {"log", NativeMods_Lua_Log},
    {"getModPath", NativeMods_Lua_GetModPath},
    {"readFile", NativeMods_Lua_ReadFile},
    {"writeFile", NativeMods_Lua_WriteFile},
    {"hook", NativeMods_Lua_Hook},
    {NULL, NULL}
};

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
    lua_setglobal(s_mods.L, "mod");

    for (int i = 0; i < NATIVE_MOD_HOOK_COUNT; i++)
        s_mods.luaRefs[i] = LUA_NOREF;

    s_mods.initialized = 1;
    fprintf(stdout, "[Mods] Lua VM initialized.\n");
    return 1;
}

void NativeMods_Shutdown(void)
{
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
        mkdir(modsDir, 0755);
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

void NativeMods_OnLanguageLoaded(char **lngStrings, int numStrings)
{
    static const char modsText[] = "MODS";

    if (lngStrings == NULL)
        return;

    if (numStrings > 0x014)
        lngStrings[0x014] = (char *)modsText;
}

FILE *NativeMods_OpenFile(const char *relativePath, const char *mode)
{
    char path[NATIVE_MODS_PATH_MAX];
    FILE *file;

    for (int i = 0; i < s_mods.modCount; i++)
    {
        if (!s_mods.mods[i].enabled)
            continue;

        int written = snprintf(path, sizeof(path), "%s/%s/%s", s_mods.mods[i].path, NATIVE_MODS_FILES_DIR, relativePath);
        if ((written <= 0) || ((size_t)written >= sizeof(path)))
            continue;

        file = fopen(path, mode);
        if (file)
            return file;
    }

    char bigfilePath[NATIVE_MODS_PATH_MAX];
    if (!NativeAssets_BuildPath(NATIVE_MODS_BIGFILE_DIR, bigfilePath, sizeof(bigfilePath)))
        return NULL;

    char unpackedPath[NATIVE_MODS_PATH_MAX];
    int written = snprintf(unpackedPath, sizeof(unpackedPath), "%s/%s", bigfilePath, relativePath);
    if ((written <= 0) || ((size_t)written >= sizeof(unpackedPath)))
        return NULL;

    file = fopen(unpackedPath, mode);
    return file;
}
