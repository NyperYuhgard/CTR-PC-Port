#!/bin/bash

cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw32.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" \
  -DCMAKE_C_FLAGS="-msse -fno-strict-aliasing -fno-inline-functions" \
  -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG"
cmake --build build-windows -j$(nproc)
