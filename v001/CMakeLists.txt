cmake_minimum_required(VERSION 3.26)

project(Qlite_world_v001 VERSION 0.1.0 LANGUAGES CXX
        DESCRIPTION "Qlite v001 — C++23 world (Qlite SQLite wrapper + Setup sensing)")

# Format checks, always on: a printf-style call whose arguments do not match its format
# (e.g. a 64-bit value given to %d) is an error, not undefined behaviour at run time.
# Conversion warnings, always on: a value silently narrowed or changing sign (size_t into int,
# int into unsigned, 64-bit into float). Warnings, not errors; -Wsign-conversion makes gcc
# report the signed/unsigned mixes clang's -Wconversion already does.
add_compile_options(-Wformat=2 -Werror=format -Wconversion -Wsign-conversion)

# Extra warnings, opt-in: the same set as jac313's JAC313_STRICT.
option(QLITE_STRICT "Extra warnings: -Wall -Wextra -Wpedantic and friends" OFF)
option(QLITE_STRICT_WERROR "With QLITE_STRICT: treat those warnings as errors" OFF)
if(QLITE_STRICT)
    # -Wnull-dereference is left out: on gcc 15 it fires only inside libstdc++ <streambuf>.
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Woverloaded-virtual
        -Wimplicit-fallthrough)
    if(QLITE_STRICT_WERROR)
        add_compile_options(-Werror)
    endif()
endif()

# Deliberate default: Debug for local development and test baselines.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug default)" FORCE)
endif()

option(QLITE_BUILD_SETUP   "Build the Setup toolchain-sensing component" ON)
option(QLITE_BUILD_MODULES "Build the C++23 modules (Qlite, Setup)" OFF)

if(QLITE_BUILD_MODULES)
    if(NOT CMAKE_GENERATOR MATCHES "Ninja")
        message(FATAL_ERROR "QLITE_BUILD_MODULES requires the Ninja generator (-G Ninja)")
    endif()
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)
    set(QLITE_SETUP_BUILD_MODULES ON CACHE BOOL "" FORCE)
endif()

enable_testing()

if(QLITE_BUILD_SETUP)
    add_subdirectory(Setup)
endif()
add_subdirectory(Qlite)

message(STATUS "Qlite v001 world configured (C++23).")
