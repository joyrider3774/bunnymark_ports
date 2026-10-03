# BunnyMark for the Gamebuino AKA (see main/bunnymark.cpp).
#
# An ESP-IDF project like every AKA game. With ESP-IDF exported, from this folder:
#   idf.py -B <build folder> build
# or from the AKA emulator: python tools/build_roms.py --only bunnymark
#
# AKA_LIB_DIR: a checkout of https://github.com/jmp42/Gamebuino_AKA_lib (default c:/github).

cmake_minimum_required(VERSION 3.16)

if(NOT AKA_LIB_DIR AND DEFINED ENV{AKA_LIB_DIR})
    set(AKA_LIB_DIR "$ENV{AKA_LIB_DIR}")
endif()
if(NOT AKA_LIB_DIR)
    set(AKA_LIB_DIR "C:/github/Gamebuino_AKA_lib")
endif()
set(AKA_LIB_DIR "${AKA_LIB_DIR}" CACHE PATH "a checkout of Gamebuino_AKA_lib")
if(NOT EXISTS "${AKA_LIB_DIR}/components/gamebuino/include_lib/gamebuino.h")
    message(FATAL_ERROR "the Gamebuino AKA library was not found in ${AKA_LIB_DIR}")
endif()
set(EXTRA_COMPONENT_DIRS "${AKA_LIB_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bunnymark)

# the library includes "gb_ll_LCD.h" where the file is gb_ll_lcd.h
set(AKA_CASE_COMPAT "${CMAKE_BINARY_DIR}/aka_case_compat")
file(WRITE "${AKA_CASE_COMPAT}/gb_ll_LCD.h"
     "#pragma once\n#include \"${AKA_LIB_DIR}/components/gamebuino/include_ll/gb_ll_lcd.h\"\n")
idf_build_set_property(COMPILE_OPTIONS "-I${AKA_CASE_COMPAT}" APPEND)
# its non-static inline fifo helpers only link where they are inlined
idf_component_get_property(aka_gamebuino_lib gamebuino COMPONENT_LIB)
target_compile_options(${aka_gamebuino_lib} PRIVATE -O2)
