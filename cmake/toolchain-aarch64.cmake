# ARM64 cross-compile toolchain for Minima node
# Target: aarch64-linux-gnu (Raspberry Pi 4/5, Android, edge devices)
#
# Usage:
#   cmake /app -DCMAKE_TOOLCHAIN_FILE=/app/cmake/toolchain-aarch64.cmake \
#              -DCMAKE_BUILD_TYPE=Release -Wno-dev
#   make -j$(nproc)
#
# Install cross-compiler:
#   Ubuntu/Debian: sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# ── Compilers ─────────────────────────────────────────────────────────────
set(TRIPLE "aarch64-linux-gnu")

find_program(CMAKE_C_COMPILER   NAMES ${TRIPLE}-gcc   gcc)
find_program(CMAKE_CXX_COMPILER NAMES ${TRIPLE}-g++   g++)

if(NOT CMAKE_C_COMPILER OR NOT CMAKE_CXX_COMPILER)
    message(FATAL_ERROR
        "ARM64 cross-compiler not found.\n"
        "Install with: sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu")
endif()

message(STATUS "ARM64 toolchain: ${CMAKE_CXX_COMPILER}")

# ── Sysroot (optional, auto-detect) ──────────────────────────────────────
# If you have a full Pi sysroot, set MINIMA_SYSROOT:
#   cmake ... -DMINIMA_SYSROOT=/path/to/sysroot
if(DEFINED MINIMA_SYSROOT AND EXISTS "${MINIMA_SYSROOT}")
    set(CMAKE_SYSROOT            "${MINIMA_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH     "${MINIMA_SYSROOT}")
    message(STATUS "Using sysroot: ${MINIMA_SYSROOT}")
endif()

# ── Find rules ────────────────────────────────────────────────────────────
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)   # host tools
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)    # target libs
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)    # target headers
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ── ABI flags ────────────────────────────────────────────────────────────
# Hard-float, no NEON requirement (safe for all Cortex-A53+)
set(CMAKE_C_FLAGS_INIT   "-march=armv8-a -mtune=cortex-a53")
set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a -mtune=cortex-a53")

# ── Static linking (for portable binary) ─────────────────────────────────
# Uncomment to produce a fully static binary (no shared lib deps):
# set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
