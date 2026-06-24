# C++23 modules and `import std;` — v001

How Qlite v001 ships an optional C++23 module, how it shares a single implementation body
with the textual header, and the `import std;` story — quirks, blessings, and curses.

> **An open invitation.** This is a small but real wrapper exercising *experimental*
> toolchain features. If you work on **CMake**, **GCC/libstdc++**, or **Clang/libc++**:
> you are welcome to use Qlite as a reproducible example to harden your `import std;` /
> C++ modules support. Friction we hit lands in [findings/](findings/README.md).

---

## Modules: headers first, modules second

Headers remain the compatibility layer; the module is **additive**. The wrapper ships an
optional C++23 module that re-exports the same public API, correctly qualified under
`Qlite::v001` via `export namespace`.

| Module | `g++-15` | `clang++-21` |
|--------|----------|--------------|
| `Qlite.v001` | _TBD_ | _TBD_ |

Module builds require the **Ninja** generator (`-DQLITE_BUILD_MODULES=ON`). Clang module
builds need a matching `clang-scan-deps`.

```bash
cmake -G Ninja -S . -B build-mod -DQLITE_BUILD_MODULES=ON
cmake --build build-mod
import Qlite.v001;   // exposes Qlite::v001::{Sqlite, SqliteError, bind_value, column, version}
```

---

## One body, two front-ends

The wrapper's single source of truth is `Sqlite.body.hpp` — an include-free fragment with
no guard. Both front-ends paste the *same* body and differ only in how `std` and
`<sqlite3.h>` names are made visible to it:

| Front-end | How `std` / `<sqlite3.h>` are made visible |
|-----------|--------------------------------------------|
| Textual header (`Sqlite.hpp`) | textual `#include <…>` of std + `<sqlite3.h>`, then the body inside `namespace Qlite::v001` |
| Module (`Qlite.v001.cppm`) | `<sqlite3.h>` in the global module fragment; the body inside `export namespace Qlite::v001` |

This is what keeps the two paths from drifting — one implementation, two visibility
strategies.

---

## `import std;` (opt-in, gcc-only)

`import std;` builds the module **module-native** against the standard-library module
instead of textually including the standard library. Notes carried over from the family's
experience:

- It uses CMake's **native** std-module support, gated behind a **CMake-version-specific
  UUID** (`CMAKE_EXPERIMENTAL_CXX_IMPORT_STD`). `≥ 3.30` is necessary but **not
  sufficient** — a different CMake version carries a different UUID and the configure
  fails. The exact pinned CMake version for v001 is **_TBD_** (owned by the build files).
- **g++ only** (guarded in CMake). Clang here uses libstdc++ and has no usable libc++ `std`
  module, so leave `import std;` OFF for clang (it keeps the textual front-end).
- Public API and behavior are identical to the default build; this only changes how the
  standard library is consumed internally.

---

## The curses (toolchain friction to watch for)

These are the family's known footguns; mark each as confirmed/not-yet-hit for v001 as we
build:

- **`export using` injects into the global namespace.** Use `export namespace` so symbols
  land under `Qlite::v001`, matching the header spelling.
- **`import std;` exports `std::` but not the global C names** (`::int64_t`, `::memcpy`,
  bare `size_t`). Qualify to `std::`, or pull C-compat headers into the global module
  fragment.
- **The CMake gate UUID changes every release.** Pin it to one exact CMake version.

_Status of each against v001: TBD — to be filled as the module path is exercised._
