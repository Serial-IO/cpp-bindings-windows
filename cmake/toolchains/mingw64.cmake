# Toolchain file for cross-compiling to 64-bit Windows using MinGW-w64 GCC
# Requires the packages: mingw64-crt mingw64-winpthreads mingw64-gcc
#
# Usage:
#   cmake -S . -B build-win \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw64.cmake \
#         -G Ninja

# Identify target platform
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# ---------- Compiler executables ----------
set(MINGW_TARGET_TRIPLE x86_64-w64-windows-gnu)
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)

# ---------- Sysroot / search paths ----------
# Default install location of mingw-w64 cross toolchain on Fedora
set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_TARGET_TRIPLE}/sys-root/mingw)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY) 
