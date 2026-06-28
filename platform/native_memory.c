#include "../platform.h"
#include "ctr_scratchpad.h"

#include <macros.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if defined(_WIN32)
#include "platform/native_win32.h"
#elif defined(__GNUC__)
#include <errno.h>
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

#ifndef CTR_NATIVE_MEMPACK_RETAIL_PRESSURE
#define CTR_NATIVE_MEMPACK_RETAIL_PRESSURE 0
#endif

// TODO(aalhendi): Re-audit LOAD_ReadFile_ex, LOAD_DramFileCallback, LEV/PTR
// callbacks, and hub swapping before removing the expanded arena escape hatch.
#if CTR_NATIVE_MEMPACK_RETAIL_PRESSURE
// NOTE(aalhendi): Retail pressure mode exposes the NTSC-U 926 mempack window
// inside a 2 MiB backing store.
#define CTR_NATIVE_MEMPACK_BUFFER_SIZE  0x200000u
#define CTR_NATIVE_MEMPACK_START_OFFSET 0xba9f0u
#define CTR_NATIVE_MEMPACK_SIZE         0x144e10u
#else
// Expanded mode: 8 MiB backing store. This gives the game ~6.7 MiB of
// allocator space (vs ~1.27 MiB in retail pressure mode), which is enough
// for netplay to load per-peer kart models without exhausting MEMPACK.
//
// IMPORTANT: the buffer must be allocated at an address < 0x01000000 for
// the 24-bit GPU primitive link packing to work. We verify this at init
// time in Platform_ConfigureMempackArena() and fall back to a smaller
// buffer (allocated via malloc, which on 32-bit builds is usually low)
// if the static buffer is too high.
#define CTR_NATIVE_MEMPACK_BUFFER_SIZE  (8u * 1024u * 1024u)
#define CTR_NATIVE_MEMPACK_START_OFFSET 0u
#define CTR_NATIVE_MEMPACK_SIZE         CTR_NATIVE_MEMPACK_BUFFER_SIZE
// Fallback size if the static buffer's address is >= 0x01000000.
// We use a malloc'd buffer with a small size that's likely to land low
// in the address space. 4 MiB is a safe compromise.
#define CTR_NATIVE_MEMPACK_FALLBACK_SIZE (4u * 1024u * 1024u)
#endif

global_variable char s_mempackMemory[CTR_NATIVE_MEMPACK_BUFFER_SIZE];
global_variable void *s_mempackFallback = NULL;
global_variable struct PlatformMempackArena s_mempackArena;

void Platform_InitScratchpad(void)
{
#if defined(CTR_NATIVE)
        void *scratchpad = (void *)CTR_SCRATCHPAD_ADDR;
        size_t scratchpadSize = CTR_SCRATCHPAD_MAP_SIZE;

#if defined(_WIN32)
        void *mapped = VirtualAlloc(scratchpad, scratchpadSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (mapped == NULL)
        {
                fprintf(stderr, "[CTR Native] Failed to map PS1 scratchpad at %p: GetLastError=%lu\n", scratchpad, GetLastError());
                abort();
        }
#elif defined(__GNUC__)
#ifdef MAP_FIXED_NOREPLACE
        int fixedFlag = MAP_FIXED_NOREPLACE;
#else
        int fixedFlag = MAP_FIXED;
#endif

        void *mapped = mmap(scratchpad, scratchpadSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | fixedFlag, -1, 0);
        if (mapped == MAP_FAILED)
        {
                fprintf(stderr, "[CTR Native] Failed to map PS1 scratchpad at %p: %s\n", scratchpad, strerror(errno));
                abort();
        }
#else
#error "Platform_InitScratchpad needs a fixed-address virtual-memory mapper for this platform"
#endif

        if (mapped != scratchpad)
        {
                fprintf(stderr, "[CTR Native] PS1 scratchpad mapped at %p, expected %p\n", mapped, scratchpad);
                abort();
        }

        memset(mapped, 0, scratchpadSize);
#endif
}

internal void Platform_ConfigureMempackArena(void)
{
        char *base = &s_mempackMemory[0];
        size_t bufferSize = CTR_NATIVE_MEMPACK_BUFFER_SIZE;

        /* The GPU primitive links pack pointers into 24 bits, so all
         * MEMPACK-allocated memory must live at addresses < 0x01000000.
         *
         * The static buffer may start low (e.g. 0xb5edc0 in BSS) but
         * extend past 0x01000000 if it's 8MB. That's fine — we just
         * clamp the usable region to 0x01000000 so MEMPACK never
         * allocates above that limit. The rest of the buffer is unused
         * but harmless.
         *
         * If the static buffer itself starts at >= 0x01000000 (unlikely
         * on 32-bit, common on 64-bit), fall back to a malloc'd buffer
         * that we hope lands lower. */
        {
                u32 baseAddr = (u32)(uintptr_t)base;
                u32 endAddr = baseAddr + (u32)bufferSize;

                if (baseAddr >= 0x01000000)
                {
                        /* Static buffer too high — try malloc fallback.
                         * On 32-bit builds, malloc usually returns low
                         * addresses that fit in 24 bits. */
                        if (s_mempackFallback == NULL)
                        {
                                s_mempackFallback = malloc(CTR_NATIVE_MEMPACK_FALLBACK_SIZE);
                                if (s_mempackFallback != NULL)
                                {
                                        memset(s_mempackFallback, 0, CTR_NATIVE_MEMPACK_FALLBACK_SIZE);
                                        fprintf(stdout, "[CTR Native] Static mempack at %p too high, "
                                                        "using malloc fallback (%u bytes at %p)\n",
                                                base, (unsigned)CTR_NATIVE_MEMPACK_FALLBACK_SIZE, s_mempackFallback);
                                }
                        }

                        if (s_mempackFallback != NULL &&
                            (u32)(uintptr_t)s_mempackFallback < 0x01000000)
                        {
                                base = (char *)s_mempackFallback;
                                bufferSize = CTR_NATIVE_MEMPACK_FALLBACK_SIZE;
                                baseAddr = (u32)(uintptr_t)base;
                                endAddr = baseAddr + (u32)bufferSize;
                        }
                }

                /* Clamp the usable region to 0x01000000. The MEMPACK
                 * allocator will never hand out addresses >= this. */
                if (endAddr > 0x01000000)
                {
                        bufferSize = (size_t)(0x01000000 - baseAddr);
                        fprintf(stdout, "[CTR Native] Mempack buffer clamped to 24-bit limit: "
                                        "base=0x%08x usable=%u bytes (of %u backing)\n",
                                baseAddr, (unsigned)bufferSize, (unsigned)CTR_NATIVE_MEMPACK_BUFFER_SIZE);
                }
        }

        s_mempackArena.base = base;
        s_mempackArena.start = base + CTR_NATIVE_MEMPACK_START_OFFSET;
        s_mempackArena.endOfMemory = base + bufferSize;
        s_mempackArena.size = bufferSize - CTR_NATIVE_MEMPACK_START_OFFSET;
        s_mempackArena.backingSize = bufferSize;

        /* lowAddressValid is true if ALL usable memory is < 0x01000000. */
        s_mempackArena.lowAddressValid =
            ((u32)(uintptr_t)s_mempackArena.base < 0x01000000) &&
            ((u32)(uintptr_t)s_mempackArena.start < 0x01000000) &&
            ((u32)(uintptr_t)s_mempackArena.endOfMemory <= 0x01000000);
}

const struct PlatformMempackArena *Platform_InitMempackArena(void)
{
        memset(s_mempackMemory, 0, sizeof(s_mempackMemory));
        Platform_ConfigureMempackArena();

        return &s_mempackArena;
}

internal void Platform_RepairResidentPointers(s32 activeMempackIndex)
{
        if ((activeMempackIndex < 0) || (activeMempackIndex >= 4))
                activeMempackIndex = 0;

        // NOTE(aalhendi): Native keeps retail-shaped global data, but pointer aliases
        // must target this process's static storage. This also moves GCC's
        // initializer-only memcard helper global out of the live state graph so
        // checkpoints capture the actual memcard buffer.
        sdata = &sdata_static;
        sdata_static.gGT = &sdata_static.gameTracker;
        sdata_static.gGamepads = &sdata_static.gamepadSystem;
        sdata_static.PtrMempack = &sdata_static.mempack[activeMempackIndex];
        sdata_static.ptrToMemcardBuffer1 = (int)&sdata_static.memcardBytes[0];
        sdata_static.ptrToMemcardBuffer2 = &sdata_static.memcardBytes[0];
}
