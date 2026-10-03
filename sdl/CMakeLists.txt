# BunnyMark for SDL2 and SDL3, from one source (bunnymark.c).
#
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# gives bunnymark_sdl2 and bunnymark_sdl3, each when that SDL is installed (found as a
# CMake package). Where a static SDL library is there and the compiler is MinGW, it is
# linked statically, so the .exe runs without SDL's DLL beside it.
#
# BUNNYMARK_SDL=2 or 3 builds only that one. USE_VENDORED_SDL downloads that SDL at a fixed
# release and builds it here as a static library, which is what tools/build_releases.py does
# on a machine without SDL (the CI runner, cross compiling for Windows with mingw-w64): it
# needs BUNNYMARK_SDL set, as the two SDLs are not built side by side in one tree.

cmake_minimum_required(VERSION 3.16)
project(BunnyMark LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(BUNNYMARK_SDL "both" CACHE STRING "which SDL to build against: 2, 3 or both")
set_property(CACHE BUNNYMARK_SDL PROPERTY STRINGS 2 3 both)
option(USE_VENDORED_SDL "download SDL and link it statically" OFF)
set(SDL2_VENDORED_TAG "release-2.32.10" CACHE STRING "SDL2 git tag USE_VENDORED_SDL builds")
set(SDL3_VENDORED_TAG "release-3.4.12" CACHE STRING "SDL3 git tag USE_VENDORED_SDL builds")

if(EMSCRIPTEN)
    # the browser build: SDL2 and SDL3 come from Emscripten's own ports (-sUSE_SDL=), there is
    # nothing to find or download here
    if(NOT BUNNYMARK_SDL STREQUAL "3")
        set(SDL2_FOUND TRUE)
    endif()
    if(NOT BUNNYMARK_SDL STREQUAL "2")
        set(SDL3_FOUND TRUE)
    endif()
elseif(USE_VENDORED_SDL)
    if(NOT BUNNYMARK_SDL MATCHES "^[23]$")
        message(FATAL_ERROR "USE_VENDORED_SDL needs BUNNYMARK_SDL set to 2 or 3")
    endif()
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG ${SDL${BUNNYMARK_SDL}_VENDORED_TAG}
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(SDL)
    set(SDL${BUNNYMARK_SDL}_FOUND TRUE)
else()
    if(NOT BUNNYMARK_SDL STREQUAL "3")
        find_package(SDL2 CONFIG QUIET)
    endif()
    if(NOT BUNNYMARK_SDL STREQUAL "2")
        find_package(SDL3 CONFIG QUIET)
    endif()
    if(NOT SDL2_FOUND AND NOT SDL3_FOUND)
        message(FATAL_ERROR "no SDL found for BUNNYMARK_SDL=${BUNNYMARK_SDL}")
    endif()
endif()

function(bunnymark_target name sdl)
    add_executable(${name} bunnymark.c)
    if(NOT MSVC)
        target_compile_options(${name} PRIVATE -Wall -Wextra -O2)
    endif()
    if(WIN32)
        set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
    if(MINGW AND (USE_VENDORED_SDL OR TARGET ${sdl}::${sdl}-static))
        target_link_options(${name} PRIVATE -static-libgcc -static)
    endif()
    if(sdl STREQUAL "SDL3")
        target_compile_definitions(${name} PRIVATE BUNNYMARK_SDL3)
    endif()
    if(EMSCRIPTEN)
        # a page (web/shell.html around the canvas) with its .js and .wasm; the million bunnies
        # (16 MB) need the memory to grow past Emscripten's default
        string(REGEX REPLACE "^SDL" "" version ${sdl})
        set_target_properties(${name} PROPERTIES SUFFIX ".html")
        target_compile_options(${name} PRIVATE -sUSE_SDL=${version})
        target_link_options(${name} PRIVATE -sUSE_SDL=${version} -sALLOW_MEMORY_GROWTH=1
            --shell-file ${PROJECT_SOURCE_DIR}/web/shell.html)
    elseif(USE_VENDORED_SDL OR (TARGET ${sdl}::${sdl}-static AND MINGW))
        target_link_libraries(${name} PRIVATE ${sdl}::${sdl}-static)
    else()
        target_link_libraries(${name} PRIVATE ${sdl}::${sdl})
    endif()
endfunction()

if(SDL2_FOUND)
    bunnymark_target(bunnymark_sdl2 SDL2)
endif()
if(SDL3_FOUND)
    bunnymark_target(bunnymark_sdl3 SDL3)
endif()
