cmake_minimum_required(VERSION 3.26)

# Native `import std;` (QLITE_IMPORT_STD) needs CMake's experimental std-module
# feature ARMED before project() enables CXX. The -D value is already in the cache
# here. UUID is pinned to the CMake version we standardize on (4.3.3); bump it if you
# move to a different CMake (see docs/Setup.md). The version-world root sets this for
# in-tree builds; this covers a standalone Qlite-component configure too.
if(QLITE_IMPORT_STD AND NOT CMAKE_EXPERIMENTAL_CXX_IMPORT_STD)
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
endif()

project(Qlite_v001 VERSION 0.1.0 LANGUAGES CXX
        DESCRIPTION "Qlite::v001 — C++23 SQLite wrapper")

# Qlite::v001 — header-only SQLite wrapper. Depends only on system SQLite3.
option(QLITE_BUILD_EXAMPLES "Build example programs" ON)
option(QLITE_BUILD_TESTS    "Build and register tests" ON)
option(QLITE_BUILD_MODULES  "Build the Qlite.v001 C++23 module" OFF)
# import-std pilot (g++ only). When ON, the module builds module-native against
# `import std;` instead of textually including the public header. Uses CMake's native
# std-module support (CMAKE_EXPERIMENTAL_CXX_IMPORT_STD gate + CXX_MODULE_STD), which
# requires CMake >= 3.30. Clang keeps the textual front-end (leave OFF).
option(QLITE_IMPORT_STD "Build the Qlite.v001 module against `import std;` (g++ pilot)" OFF)

find_package(SQLite3 REQUIRED)
# FindSQLite3 renamed its imported target SQLite::SQLite3 -> SQLite3::SQLite3 in newer
# CMake (old name still works but warns). Prefer the new name when present.
if(TARGET SQLite3::SQLite3)
    set(QLITE_SQLITE3_TARGET SQLite3::SQLite3)
else()
    set(QLITE_SQLITE3_TARGET SQLite::SQLite3)
endif()

add_library(Qlite_v001 INTERFACE)
add_library(Qlite::Qlite ALIAS Qlite_v001)

target_include_directories(Qlite_v001 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(Qlite_v001 INTERFACE cxx_std_23)
target_link_libraries(Qlite_v001 INTERFACE ${QLITE_SQLITE3_TARGET})

if(QLITE_BUILD_MODULES)
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)

    add_library(qlite_v001_module OBJECT)
    add_library(Qlite::Qlite_module ALIAS qlite_v001_module)
    target_sources(qlite_v001_module
        PUBLIC
            FILE_SET cxx_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            FILES modules/Qlite.v001.cppm
    )
    target_link_libraries(qlite_v001_module PUBLIC Qlite_v001)

    if(QLITE_IMPORT_STD)
        if(CMAKE_VERSION VERSION_LESS 3.30)
            message(FATAL_ERROR
                "QLITE_IMPORT_STD needs CMake >= 3.30 for native `import std;` "
                "(have ${CMAKE_VERSION}). See docs/Setup.md.")
        endif()
        if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            message(FATAL_ERROR
                "QLITE_IMPORT_STD is a g++ pilot; compiler is ${CMAKE_CXX_COMPILER_ID}. "
                "Leave it OFF for clang (textual front-end).")
        endif()
        set_target_properties(qlite_v001_module PROPERTIES CXX_MODULE_STD ON)
        target_compile_features(qlite_v001_module PUBLIC cxx_std_23)
        target_compile_definitions(qlite_v001_module PRIVATE QLITE_IMPORT_STD)
        message(STATUS "Qlite::v001 module: import std ENABLED (native CXX_MODULE_STD)")
    else()
        target_compile_features(qlite_v001_module PUBLIC cxx_std_23)
    endif()
endif()

if(QLITE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(QLITE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(QLITE_BUILD_MODULES AND QLITE_BUILD_TESTS)
    add_subdirectory(tests/module)
endif()

message(STATUS "Qlite::v001 configured (C++23 SQLite wrapper)")
