set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

if(NOT MINGW32_PREFIX)
    set(_MINGW_CANDIDATES
        "$ENV{HOME}/mingw32/usr/bin/i686-w64-mingw32-"
        "/usr/bin/i686-w64-mingw32-"
    )
    foreach(_prefix ${_MINGW_CANDIDATES})
        if(EXISTS "${_prefix}gcc")
            set(MINGW32_PREFIX "${_prefix}")
            break()
        endif()
    endforeach()
    if(NOT MINGW32_PREFIX)
        find_program(_MINGW_GCC i686-w64-mingw32-gcc)
        if(_MINGW_GCC)
            get_filename_component(_MINGW_DIR "${_MINGW_GCC}" DIRECTORY)
            set(MINGW32_PREFIX "${_MINGW_DIR}/i686-w64-mingw32-")
        endif()
    endif()
endif()

if(NOT MINGW32_PREFIX)
    message(FATAL_ERROR "i686-w64-mingw32-gcc not found. Install: sudo apt install gcc-mingw-w64-i686 g++-mingw-w64-i686-posix")
endif()

message(STATUS "Using MinGW32 prefix: ${MINGW32_PREFIX}")

set(CMAKE_C_COMPILER   "${MINGW32_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${MINGW32_PREFIX}g++")
set(CMAKE_RC_COMPILER  "${MINGW32_PREFIX}windres")
set(CMAKE_AR           "${MINGW32_PREFIX}ar")
set(CMAKE_RANLIB       "${MINGW32_PREFIX}ranlib")
set(CMAKE_STRIP        "${MINGW32_PREFIX}strip")

if(EXISTS "${MINGW32_PREFIX}../i686-w64-mingw32")
    get_filename_component(_MINGW_BINDIR "${MINGW32_PREFIX}" DIRECTORY)
    set(CMAKE_FIND_ROOT_PATH "${_MINGW_BINDIR}/../i686-w64-mingw32")
else()
    set(CMAKE_FIND_ROOT_PATH "/usr/i686-w64-mingw32")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(WIN32 TRUE)
set(MINGW TRUE)
