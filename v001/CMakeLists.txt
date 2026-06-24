cmake_minimum_required(VERSION 3.26)

# Native `import std;` (any *_IMPORT_STD opt-in) needs CMake's experimental std-module
# feature ARMED before project() enables CXX. UUID is pinned to CMake 4.3.3 (see
# docs/Setup.md); bump it for a different CMake. Only armed when a build opts in.
if(QLITE_IMPORT_STD OR QLITE_SETUP_IMPORT_STD)
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
endif()

project(Qlite_world_v001 VERSION 0.1.0 LANGUAGES CXX
        DESCRIPTION "Qlite v001 — C++23 world (Qlite SQLite wrapper + Setup sensing)")

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
