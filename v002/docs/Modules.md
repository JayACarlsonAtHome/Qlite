# C++ modules — v002

How Qlite v002 ships an optional C++ module, how it shares a single implementation body
with the textual header, and the module pitfalls to watch for.
v002 is the **C++26** world (see [Cpp26_Adoption.md](Cpp26_Adoption.md)).

> **An open invitation.** This is a small but real wrapper exercising *experimental*
> toolchain features at C++26. If you work on **CMake**, **GCC/libstdc++**, or
> **Clang/libc++**: you are welcome to use Qlite as a reproducible example to harden your
> C++ modules support. Friction we hit lands in [findings/](findings/README.md).

---

## Modules: headers first, modules second

Headers remain the compatibility layer; the module is **additive**. The wrapper ships an
optional C++ module that re-exports the same public API, correctly qualified under
`Qlite::v002` via `export namespace`.

| Module | `g++-15` | `clang++-21` |
|--------|----------|--------------|
| `Qlite.v002` | _TBD_ | _TBD_ |

Module builds require the **Ninja** generator and CMake >= 3.28 (`-DQLITE_BUILD_MODULES=ON`). Clang module
builds need a matching `clang-scan-deps`.

```bash
cmake -G Ninja -S . -B build-mod -DQLITE_BUILD_MODULES=ON
cmake --build build-mod
import Qlite.v002;   // exposes Qlite::v002::{Sqlite, SqliteError, bind_value, column, version}
```

---

## One body, two front-ends

The wrapper's single source of truth is `Sqlite.body.hpp` — an include-free fragment with
no guard. Both front-ends paste the *same* body and differ only in how `std` and
`<sqlite3.h>` names are made visible to it:

| Front-end | How `std` / `<sqlite3.h>` are made visible |
|-----------|--------------------------------------------|
| Textual header (`Sqlite.hpp`) | textual `#include <…>` of std + `<sqlite3.h>`, then the body inside `namespace Qlite::v002` |
| Module (`Qlite.v002.cppm`) | `<sqlite3.h>` in the global module fragment; the body inside `export namespace Qlite::v002` |

This is what keeps the two paths from drifting — one implementation, two visibility
strategies. The C++26 contracts shim flows through the body the same way (see
[Cpp26_Adoption.md](Cpp26_Adoption.md)).

---

## The curses (toolchain friction to watch for)

These are the family's known footguns; mark each as confirmed/not-yet-hit for v002 — and
note that the **C++26 standard** path can surface its own (see
[Cpp26_Adoption.md](Cpp26_Adoption.md) and [findings/](findings/README.md)):

- **`export using` injects into the global namespace.** Use `export namespace` so symbols
  land under `Qlite::v002`, matching the header spelling.
- **C++26-only constant-eval / format defects.** A spec the C++23 path accepts can break at
  C++26 on one compiler — isolate to a [finding](findings/README.md).

_Status of each against v002: TBD — to be filled as the module path is exercised._
