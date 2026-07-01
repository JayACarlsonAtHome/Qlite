module;

// Qlite.v001 — module interface over the shared implementation body
// (Sqlite.body.hpp). Both build configurations export the SAME public API under
// `Qlite::v001` (matching the textual header); the ONLY difference is how the std
// library is made visible to the body:
//
//   QLITE_IMPORT_STD  ON  -> `import std;`            (g++ pilot)
//                     OFF -> textual std #includes    (clang / fallback)
//
// <sqlite3.h> (a C library) is always textual in the global module fragment.
// The body is defined in the module purview and exported via `export namespace`,
// which gives the entities module linkage while still exposing them, qualified,
// to importers — `export using` would instead inject the names into the global
// namespace, diverging from the header's `Qlite::v001::` spelling.

#include <sqlite3.h>

#ifndef QLITE_IMPORT_STD
#  include <string>
#  include <string_view>
#  include <stdexcept>
#  include <utility>
#  include <type_traits>
#  include <cstddef>
#  include <cstdint>
#  include <optional>
#  include <tuple>
#  include <vector>
#  include <span>
#  include <unordered_map>
#  include <iostream>
#  if __has_include(<expected>)
#    include <expected>
#  endif
#endif

export module Qlite.v001;

#ifdef QLITE_IMPORT_STD
import std;
#endif

export namespace Qlite::v001 {
#include "Qlite/v001/Sqlite.body.hpp"
}
