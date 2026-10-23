# C++23 modules — v001

How Qlite v001 ships an optional C++23 module, how it shares a single implementation body
with the textual header, and the module pitfalls to watch for.

> **An open invitation.** This is a small but real wrapper exercising *experimental*
> toolchain features. If you work on **CMake**, **GCC/libstdc++**, or **Clang/libc++**:
> you are welcome to use Qlite as a reproducible example to harden your
> C++ modules support. Friction we hit lands in [findings/](findings/README.md).

---

## Modules: headers first, modules second

Headers remain the compatibility layer; the module is **additive**. The wrapper ships an
optional C++23 module that re-exports the same public API, correctly qualified under
`Qlite::v001` via `export namespace`.

| Module | `g++-15` | `clang++-21` |
|--------|----------|--------------|
| `Qlite.v001` | _TBD_ | _TBD_ |

Module builds require the **Ninja** generator and CMake >= 3.28 (`-DQLITE_BUILD_MODULES=ON`). Clang module
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

## The curses (toolchain friction to watch for)

These are the family's known footguns; mark each as confirmed/not-yet-hit for v001 as we
build:

- **`export using` injects into the global namespace.** Use `export namespace` so symbols
  land under `Qlite::v001`, matching the header spelling.

_Status of each against v001: TBD — to be filled as the module path is exercised._
