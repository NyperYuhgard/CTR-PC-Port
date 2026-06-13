#!/bin/bash
cmake -S . -B build-windows -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw32.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-windows -j$(nproc)
