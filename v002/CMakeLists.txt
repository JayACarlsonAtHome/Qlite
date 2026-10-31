cmake_minimum_required(VERSION 3.26)

project(Qlite_world_v002 VERSION 0.1.0 LANGUAGES CXX
        DESCRIPTION "Qlite v002 — C++26 world (Qlite SQLite wrapper + Setup sensing)")

# Format checks, always on: a printf-style call whose arguments do not match its format
# (e.g. a 64-bit value given to %d) is an error, not undefined behaviour at run time.
# Conversion warnings, always on: a value silently narrowed or changing sign (size_t into int,
# int into unsigned, 64-bit into float). -Wsign-conversion makes gcc
# report the signed/unsigned mixes clang's -Wconversion already does.
# Strict warnings, always on, as in jac313: no runtime effect.
# -Wnull-dereference is left out: on gcc 15 it fires only inside libstdc++ <streambuf>.
# -Werror, always on: every warning is a build error, so none can pile up unseen.
add_compile_options(-Werror -Wformat=2 -Wconversion -Wsign-conversion
    -Wall -Wextra -Wpedantic
    -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Woverloaded-virtual
    -Wimplicit-fallthrough)

# Deliberate default: Debug for local development and test baselines.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug default)" FORCE)
endif()

option(QLITE_BUILD_SETUP   "Build the Setup toolchain-sensing component" ON)
option(QLITE_BUILD_MODULES "Build the C++26 modules (Qlite, Setup)" OFF)

if(QLITE_BUILD_MODULES)
    if(NOT CMAKE_GENERATOR MATCHES "Ninja")
        message(FATAL_ERROR "QLITE_BUILD_MODULES requires the Ninja generator (-G Ninja)")
    endif()
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)
    set(QLITE_SETUP_BUILD_MODULES ON CACHE BOOL "" FORCE)
endif()

# Contracts (Qlite/include/Qlite/v002/contracts.hpp): GCC 16 builds with NATIVE C++26 contracts,
# so QLITE_PRE/POST/ASSERT are contract_assert under the 'enforce' semantic (a violation
# terminates); other compilers keep the macros' runtime checks. -fcontracts on the link line
# too, or gcc16 leaves out the default contract-violation handler.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16)
    add_compile_options(-fcontracts -fcontract-evaluation-semantic=enforce)
    add_link_options(-fcontracts)
endif()

enable_testing()

if(QLITE_BUILD_SETUP)
    add_subdirectory(Setup)
endif()
add_subdirectory(Qlite)

message(STATUS "Qlite v002 world configured (C++26).")
