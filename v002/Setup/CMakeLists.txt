cmake_minimum_required(VERSION 3.26)
project(Setup_v002 VERSION 0.1.0 LANGUAGES CXX)

# Setup::Setup — standalone, data-driven, activation-aware toolchain sensing
# (Setup::v002). Self-contained: its own command-runner, no deps beyond the
# C++23 standard library, so it can be reused by any build tooling and via the module
# `import Qlite.setup.v002;`. Must build before consumers.

# Experimental `import std` pilot guard (CMake EXPERIMENTAL UUID). Kept identical to the
# jac313 source so the same opt-in works here.
set(QLITE_SETUP_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

option(QLITE_SETUP_BUILD_TESTS "Build and register tests" OFF)
option(QLITE_SETUP_BUILD_MODULES "Build Qlite.setup.v002 C++23 module" OFF)

add_library(Setup_v002 STATIC
    src/v002.cpp
)
add_library(Setup::Setup ALIAS Setup_v002)

target_include_directories(Setup_v002 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(Setup_v002 PUBLIC cxx_std_26)

if(QLITE_SETUP_BUILD_MODULES)
    if(NOT CMAKE_GENERATOR MATCHES "Ninja")
        message(FATAL_ERROR "QLITE_SETUP_BUILD_MODULES requires the Ninja generator (-G Ninja)")
    endif()

    add_library(setup_v002_module OBJECT)
    add_library(Setup::Setup_module ALIAS setup_v002_module)
    target_sources(setup_v002_module
        PUBLIC
            FILE_SET cxx_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            FILES modules/Qlite.setup.v002.cppm
    )
    target_include_directories(setup_v002_module PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_features(setup_v002_module PUBLIC cxx_std_26)
    target_link_libraries(setup_v002_module PUBLIC Setup_v002)
endif()

if(QLITE_SETUP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "Setup::Setup configured (standalone toolchain sensing, v002)")
