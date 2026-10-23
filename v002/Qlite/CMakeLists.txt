cmake_minimum_required(VERSION 3.26)

project(Qlite_v002 VERSION 0.1.0 LANGUAGES CXX
        DESCRIPTION "Qlite::v002 — C++26 SQLite wrapper")

# Qlite::v002 — header-only SQLite wrapper. Depends only on system SQLite3.
option(QLITE_BUILD_EXAMPLES "Build example programs" ON)
option(QLITE_BUILD_TESTS    "Build and register tests" ON)
option(QLITE_BUILD_MODULES  "Build the Qlite.v002 C++26 module" OFF)

find_package(SQLite3 REQUIRED)
# FindSQLite3 renamed its imported target SQLite::SQLite3 -> SQLite3::SQLite3 in newer
# CMake (old name still works but warns). Prefer the new name when present.
if(TARGET SQLite3::SQLite3)
    set(QLITE_SQLITE3_TARGET SQLite3::SQLite3)
else()
    set(QLITE_SQLITE3_TARGET SQLite::SQLite3)
endif()

add_library(Qlite_v002 INTERFACE)
add_library(Qlite::Qlite ALIAS Qlite_v002)

target_include_directories(Qlite_v002 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(Qlite_v002 INTERFACE cxx_std_26)
target_link_libraries(Qlite_v002 INTERFACE ${QLITE_SQLITE3_TARGET})

if(QLITE_BUILD_MODULES)
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)

    add_library(qlite_v002_module OBJECT)
    add_library(Qlite::Qlite_module ALIAS qlite_v002_module)
    target_sources(qlite_v002_module
        PUBLIC
            FILE_SET cxx_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            FILES modules/Qlite.v002.cppm
    )
    target_link_libraries(qlite_v002_module PUBLIC Qlite_v002)

    target_compile_features(qlite_v002_module PUBLIC cxx_std_26)
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

message(STATUS "Qlite::v002 configured (C++26 SQLite wrapper)")
