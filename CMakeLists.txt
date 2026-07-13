cmake_minimum_required(VERSION 3.20)
project(CTR-Native C)

set(CMAKE_C_STANDARD 99)
set(CTR_NATIVE_VERSION "0.1.0-beta.3")

if(NOT CMAKE_C_COMPILER_TARGET MATCHES "i.86" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "i.86")
    set(CMAKE_C_FLAGS "-m32 ${CMAKE_C_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "-m32 ${CMAKE_EXE_LINKER_FLAGS}")
endif()

set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc ${CMAKE_EXE_LINKER_FLAGS}")

if(CMAKE_CROSSCOMPILING)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
else()
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
endif()

execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE CTR_NATIVE_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT CTR_NATIVE_GIT_HASH)
    set(CTR_NATIVE_GIT_HASH "unknown")
endif()

execute_process(
    COMMAND git diff --quiet --exit-code
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE CTR_NATIVE_GIT_DIRTY
    ERROR_QUIET
)
if(CTR_NATIVE_GIT_DIRTY EQUAL 0)
    set(CTR_NATIVE_BUILD_ID "${CTR_NATIVE_GIT_HASH}")
else()
    string(TIMESTAMP CTR_NATIVE_BUILD_TIMESTAMP "%Y%m%d%H%M%S" UTC)
    set(CTR_NATIVE_BUILD_ID "${CTR_NATIVE_GIT_HASH}-dirty-${CTR_NATIVE_BUILD_TIMESTAMP}")
endif()

set(GAME_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse -Wno-error -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types -g")

# SDL3
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
# SDL_RENDER stays OFF — the netplay chat window deliberately does NOT use
# an SDL renderer (it would conflict with the game's main OpenGL context
# and crash Mesa/Intel drivers). The chat window exists only to capture
# keyboard input; content is shown in the game's HUD and terminal instead.
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
set(SDL_TRAY OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_OFFSCREEN OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
if(WIN32)
    set(SDL_DIRECTX ON CACHE BOOL "" FORCE)
    set(SDL_WASAPI ON CACHE BOOL "" FORCE)
    set(SDL_XINPUT ON CACHE BOOL "" FORCE)
    set(SDL_HIDAPI ON CACHE BOOL "" FORCE)
    set(SDL_ALSA OFF CACHE BOOL "" FORCE)
    set(SDL_PULSEAUDIO OFF CACHE BOOL "" FORCE)
    set(SDL_PIPEWIRE OFF CACHE BOOL "" FORCE)
    set(SDL_JACK OFF CACHE BOOL "" FORCE)
    set(SDL_X11 OFF CACHE BOOL "" FORCE)
    set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
    set(SDL_LIBUSB OFF CACHE BOOL "" FORCE)
    set(SDL_DBUS OFF CACHE BOOL "" FORCE)
    set(SDL_UDEV OFF CACHE BOOL "" FORCE)
    set(SDL_RPI OFF CACHE BOOL "" FORCE)
    set(SDL_KMSDRM OFF CACHE BOOL "" FORCE)
endif()
add_subdirectory(externals/SDL)

# Lua
add_subdirectory(externals/lua)

# ENet
add_library(enet STATIC
    externals/enet/callbacks.c
    externals/enet/compress.c
    externals/enet/host.c
    externals/enet/list.c
    externals/enet/packet.c
    externals/enet/peer.c
    externals/enet/protocol.c
    externals/enet/unix.c
)
target_include_directories(enet PUBLIC externals/enet/include)
if(WIN32)
    target_sources(enet PRIVATE externals/enet/win32.c)
    target_link_libraries(enet ws2_32)
endif()

# CTR Native
add_executable(ctr_native main.c)
target_include_directories(ctr_native PRIVATE ${GAME_INCLUDES})
option(CTR_NATIVE_DEV_GHOST "Enable Save Dev Ghost menu option (developer-only)" OFF)
option(CTR_ONLINE "Enable Online menu (developer-only)" OFF)
target_compile_definitions(ctr_native PRIVATE
    CTR_NATIVE
    BUILD=926
    CTR_INTERNAL
    CTR_NATIVE_VERSION=\"${CTR_NATIVE_VERSION}\"
    CTR_NATIVE_BUILD_ID=\"${CTR_NATIVE_BUILD_ID}\"
)
if(CTR_NATIVE_DEV_GHOST)
    target_compile_definitions(ctr_native PRIVATE CTR_NATIVE_DEV_GHOST)
endif()
if(CTR_ONLINE)
    target_compile_definitions(ctr_native PRIVATE CTR_ONLINE)
endif()
target_compile_options(ctr_native PRIVATE -O2)
target_link_libraries(ctr_native SDL3::SDL3 lua enet)
target_include_directories(ctr_native PRIVATE externals/enet/include)
if(WIN32)
    target_link_options(ctr_native PRIVATE -static -static-libgcc -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase -Wl,--disable-reloc-section)
    target_link_libraries(ctr_native ws2_32 iphlpapi)
else()
    target_link_options(ctr_native PRIVATE -static-libgcc -Wl,--image-base,0x400000 -no-pie -Wl,--allow-multiple-definition)
endif()

# CTR-Netplay Server (standalone dedicated server)
add_executable(ctr_server platform/netplay_server.c)
target_include_directories(ctr_server PRIVATE externals/enet/include include)
target_link_libraries(ctr_server enet)
target_compile_options(ctr_server PRIVATE -O2 -g)
if(WIN32)
    target_link_options(ctr_server PRIVATE -static -static-libgcc)
else()
    target_link_options(ctr_server PRIVATE -static-libgcc)
endif()
