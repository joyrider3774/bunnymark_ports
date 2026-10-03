# Builds the Windows exes of the SDL port on Linux with mingw-w64, for the CI workflow.
# tools/build_releases.py passes this with --cross-windows.
#
# Ubuntu has the compilers in the mingw-w64 package. SDL itself is downloaded and built by the
# SDL port's CMakeLists (USE_VENDORED_SDL), so nothing else needs to be installed.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
# the compilers are the host's, everything else is looked for in the Windows sysroot and in whatever
# CMAKE_PREFIX_PATH adds (SDL2)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
