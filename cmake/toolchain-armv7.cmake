# ARMv7 cross-compile toolchain for Minima node
# Target: armv7-linux-gnueabihf (Raspberry Pi 2/3, older Android, IoT)
#
# Usage:
#   cmake /app -DCMAKE_TOOLCHAIN_FILE=/app/cmake/toolchain-armv7.cmake \
#              -DCMAKE_BUILD_TYPE=Release -Wno-dev
#
# Install:
#   Ubuntu/Debian: sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7)

set(TRIPLE "arm-linux-gnueabihf")

find_program(CMAKE_C_COMPILER   NAMES ${TRIPLE}-gcc   gcc)
find_program(CMAKE_CXX_COMPILER NAMES ${TRIPLE}-g++   g++)

if(NOT CMAKE_C_COMPILER OR NOT CMAKE_CXX_COMPILER)
    message(FATAL_ERROR
        "ARMv7 cross-compiler not found.\n"
        "Install with: sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf")
endif()

message(STATUS "ARMv7 toolchain: ${CMAKE_CXX_COMPILER}")

if(DEFINED MINIMA_SYSROOT AND EXISTS "${MINIMA_SYSROOT}")
    set(CMAKE_SYSROOT        "${MINIMA_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH "${MINIMA_SYSROOT}")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Hard-float ABI, Thumb-2 — safe for Cortex-A7/A9
set(CMAKE_C_FLAGS_INIT   "-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -mthumb")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -mthumb")
