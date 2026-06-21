#define _CRT_SECURE_NO_WARNINGS
#define SDL_MAIN_HANDLED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#if __GNUC__
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#define _EnterCriticalSection(x)
#define EnterCriticalSection(x)
#define ExitCriticalSection()
#endif

#include "psx/types.h"
#include "psx/libetc.h"
#include "psx/libgte.h"
#include "psx/libgpu.h"
#include "psx/libspu.h"
#include "psx/libcd.h"
#include "psx/libapi.h"
#include "psx/strings.h"
#include "psx/inline_c.h"
#include "platform/native_assets.h"
#include "platform/native_bigfile.h"
#include "platform/native_log.h"
#include "platform/native_perf.h"
#include "platform/native_replay_scheduler.h"
#include "platform/native_savestate.h"
#include "platform/native_mods.h"
#include "platform/native_netplay.h"

#ifndef __GNUC__
#define _Static_assert(x)
#define __attribute__(x)
#endif

#define RECT RECT16
typedef enum
{
        PAD_ID_MOUSE = 0x1,
        PAD_ID_NEGCON = 0x2,
        PAD_ID_IRQ10_GUN = 0x3,
        PAD_ID_DIGITAL = 0x4,
        PAD_ID_ANALOG_STICK = 0x5,
        PAD_ID_GUNCON = 0x6,
        PAD_ID_ANALOG = 0x7,
        PAD_ID_MULTITAP = 0x8,
        PAD_ID_JOGCON = 0xe,
        PAD_ID_CONFIG_MODE = 0xf,
        PAD_ID_NONE = 0xf
} PadTypeID;

#include "platform.h"

#include "game_includes.h"

#include "game/zGlobal_RDATA.c"
#include "game/zGlobal_DATA.c"
#include "game/zGlobal_SDATA.c"

#undef RECT

#include "platform/native_assets.c"
#include "platform/native_bigfile.c"
#include "platform/native_audio.c"
#include "platform/native_memory.c"
#include "platform/native_checkpoint.c"
#include "platform/native_checkpoint_file.c"
#include "platform/native_cd.c"
#include "platform/native_gpu.c"
#include "platform/native_gte_core.c"
#include "platform/native_glad.c"
#include "platform/native_input.c"
#include "platform/native_inline_c.c"
#include "platform/native_libapi.c"
#include "platform/native_libetc.c"
#include "platform/native_libgte.c"
#include "platform/native_libgpu.c"
#include "platform/native_libpad.c"
#include "platform/native_libspu.c"
#include "platform/native_log.c"
#include "platform/native_memcard.c"
#include "platform/native_perf.c"
#include "platform/native_platform.c"
#include "platform/native_replay_scheduler.c"
#include "platform/native_renderer.c"
#include "platform/native_savestate.c"
#include "platform/native_state.c"
#include "platform/native_str.c"
#include "platform/native_mods.c"
#include "platform/native_netplay.c"

#ifndef CC
#if __GNUC__
#if _WIN32
#ifndef __clang__
#define CC "MINGW-GCC"
#else
#define CC "MINGW-CLANG"
#endif
#else
#ifndef __clang__
#define CC "GCC"
#else
#define CC "CLANG"
#endif
#endif
#elif defined(_MSC_VER)
#define CC "MSVC"
#else
#define CC "Unknown"
#endif
#endif

#ifndef CTR_NATIVE_VERSION
#define CTR_NATIVE_VERSION "0.0.0-dev"
#endif

#ifndef CTR_NATIVE_BUILD_ID
#define CTR_NATIVE_BUILD_ID "unknown"
#endif

static int NativeConsole_ShouldPauseOnError(void)
{
#if defined(_WIN32)
        DWORD consoleProcesses[2];
        DWORD consoleProcessCount;

        if (GetConsoleWindow() == NULL)
                return 0;

        consoleProcessCount = GetConsoleProcessList(consoleProcesses, (DWORD)(sizeof(consoleProcesses) / sizeof(consoleProcesses[0])));
        return (consoleProcessCount == 1) && (consoleProcesses[0] == GetCurrentProcessId());
#else
        return 0;
#endif
}

static int NativeConsole_Return(int result)
{
        if ((result != 0) && NativeConsole_ShouldPauseOnError())
        {
                fflush(stdout);
                fflush(stderr);
                fprintf(stderr, "\n[CTR Native] Press Enter to close this window...");
                fflush(stderr);

                while (getchar() != '\n' && !feof(stdin))
                {
                }
        }

        return result;
}

// TODO(aalhendi): just make an argparser?
static int NativeArg_IsVersion(const char *arg)
{
        return (arg != NULL) && ((strcmp(arg, "--version") == 0) || (strcmp(arg, "-v") == 0));
}

struct NativeNetplayArgs
{
        int  enable;
        int  isHost;
        char connectAddress[64];
        u16  port;
        char playerName[32];
        int  playerCount;
};

internal void NativeArg_DefaultNetplayArgs(struct NativeNetplayArgs *args)
{
        args->enable = 0;
        args->isHost = 0;
        memset(args->connectAddress, 0, sizeof(args->connectAddress));
        args->port = NETPLAY_DEFAULT_PORT;
        memset(args->playerName, 0, sizeof(args->playerName));
        args->playerCount = 2;
        snprintf(args->playerName, sizeof(args->playerName) - 1, "Player");
}

int main(int argc, char *argv[])
{
        struct NativeNetplayArgs netplayArgs;

        NativeArg_DefaultNetplayArgs(&netplayArgs);

        for (int argIndex = 1; argIndex < argc; argIndex++)
        {
                if (NativeArg_IsVersion(argv[argIndex]))
                {
                        printf("CTR Native %s (%s)\n", CTR_NATIVE_VERSION, CTR_NATIVE_BUILD_ID);
                        return 0;
                }

                if (strcmp(argv[argIndex], "--host") == 0)
                {
                        netplayArgs.enable = 1;
                        netplayArgs.isHost = 1;
                        if (argIndex + 1 < argc && argv[argIndex + 1][0] >= '0' && argv[argIndex + 1][0] <= '9')
                        {
                                netplayArgs.port = (u16)atoi(argv[++argIndex]);
                        }
                }
                else if (strcmp(argv[argIndex], "--connect") == 0)
                {
                        if (argIndex + 1 < argc)
                        {
                                netplayArgs.enable = 1;
                                netplayArgs.isHost = 0;

                                const char *addrArg = argv[++argIndex];
                                const char *colon = strchr(addrArg, ':');

                                if (colon != NULL)
                                {
                                        size_t addrLen = (size_t)(colon - addrArg);
                                        if (addrLen >= sizeof(netplayArgs.connectAddress))
                                                addrLen = sizeof(netplayArgs.connectAddress) - 1;
                                        memcpy(netplayArgs.connectAddress, addrArg, addrLen);
                                        netplayArgs.connectAddress[addrLen] = '\0';
                                        netplayArgs.port = (u16)atoi(colon + 1);
                                }
                                else
                                {
                                        strncpy(netplayArgs.connectAddress, addrArg, sizeof(netplayArgs.connectAddress) - 1);
                                }
                        }
                }
                else if (strcmp(argv[argIndex], "--name") == 0)
                {
                        if (argIndex + 1 < argc)
                        {
                                strncpy(netplayArgs.playerName, argv[++argIndex], sizeof(netplayArgs.playerName) - 1);
                        }
                }
                else if (strcmp(argv[argIndex], "--players") == 0)
                {
                        if (argIndex + 1 < argc)
                        {
                                int count = atoi(argv[++argIndex]);
                                if (count >= 2 && count <= NETPLAY_MAX_PLAYERS)
                                        netplayArgs.playerCount = count;
                        }
                }
        }

        printf("[CTR Native] Starting...\n");
        fflush(stdout);

        const char *sdlBasePath = SDL_GetBasePath();
        printf("[CTR Native] SDL base path: %s\n", sdlBasePath ? sdlBasePath : "(null)");
        fflush(stdout);

        if (!NativeAssets_Init(sdlBasePath))
        {
                fprintf(stderr, "[CTR Native] Failed to initialize asset paths.\n");
                return NativeConsole_Return(1);
        }

        printf("[CTR Native] Version: %s (%s)\n", CTR_NATIVE_VERSION, CTR_NATIVE_BUILD_ID);
        printf("[CTR Native] Built with: " CC "\n");
        printf("[CTR Native] Base: %s\n", NativeAssets_GetBaseDir());
        printf("[CTR Native] Assets: %s\n", NativeAssets_GetAssetDir());
        fflush(stdout);

        if (chdir(NativeAssets_GetBaseDir()) != 0)
        {
                fprintf(stderr, "[CTR Native] Failed to enter base directory: %s\n", NativeAssets_GetBaseDir());
                return NativeConsole_Return(1);
        }

        if (!NativeAssets_Validate())
                return NativeConsole_Return(1);

        if (!NativeMods_Init())
                fprintf(stderr, "[CTR Native] Failed to initialize mod system.\n");
        else
        {
                NativeMods_ScanMods();
                NativeMods_LoadModScripts();
        }

#if defined(CTR_INTERNAL)
        if (NativeReplayScheduler_PrepareReportFromArgs(argc, argv) != 0)
                return NativeConsole_Return(1);
#endif

#ifdef USE_16BY9
        printf("[CTR Native] Widescreen\n");
        Platform_Init("Crash Team Racing", 1280, 720);
#else
        printf("[CTR Native] 4:3\n");
        Platform_Init("Crash Team Racing", 800, 600);
#endif

#if defined(CTR_INTERNAL)
        if (NativePerf_ConfigureFromArgs(argc, argv) != 0)
        {
                Platform_LogFlush();
                Platform_Shutdown();
                return NativeConsole_Return(1);
        }
#endif

        Platform_InitScratchpad();
        Platform_RepairResidentPointers(0);

	// Initialize netplay if requested
	if (netplayArgs.enable)
	{
		Netplay_SetPlayerName(netplayArgs.playerName);
		Netplay_Init();

		if (netplayArgs.isHost)
		{
			if (!Netplay_Host(netplayArgs.port))
			{
				fprintf(stderr, "[CTR Native] Failed to host netplay on port %u\n", netplayArgs.port);
				Netplay_Shutdown();
			}
			else
			{
				printf("[CTR Native] Netplay host on port %u, waiting for %d players...\n",
				       netplayArgs.port, netplayArgs.playerCount);
			}
		}
		else if (netplayArgs.connectAddress[0] != '\0')
		{
			if (!Netplay_Connect(netplayArgs.connectAddress, netplayArgs.port))
			{
				fprintf(stderr, "[CTR Native] Failed to connect to %s:%u\n",
				        netplayArgs.connectAddress, netplayArgs.port);
				Netplay_Shutdown();
			}
			else
			{
				printf("[CTR Native] Netplay connecting to %s:%u...\n",
				       netplayArgs.connectAddress, netplayArgs.port);
			}
		}

		g_NetplayAutoJoin = 1;
		fflush(stdout);
	}

#if defined(CTR_INTERNAL)
        if (NativeReplayScheduler_ConfigureFromArgs(argc, argv) != 0)
        {
                Platform_LogFlush();
                Platform_Shutdown();
                return NativeConsole_Return(1);
        }
#else
        (void)argc;
        (void)argv;
#endif

        int result = CTR_Main();

        if (Netplay_GetState() != NETPLAY_STATE_DISCONNECTED)
                Netplay_Disconnect();
        Netplay_Shutdown();
        NativeMods_Shutdown();
        Platform_Shutdown();
        return NativeConsole_Return(result);
}
