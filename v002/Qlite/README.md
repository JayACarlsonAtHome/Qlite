# Qlite::v002

The **C++26** wrapper component of the [v002 world](../README.md).

`Qlite::v002` is a **header-only** SQLite3 wrapper — the canonical evolution of the legacy
`jacQlite` / `jac::qlite` header-only wrapper, built on **real C++26** (a `contracts`
pre/post/assert shim, etc. — see [Cpp26_Adoption.md](../docs/Cpp26_Adoption.md)). It
depends only on system SQLite3. Public symbols: `Sqlite`, `SqliteError`, `bind_value`,
`column`, `version`.

`version()` returns the package's `"major.minor"` version: major is the `v002` API line,
minor bumps on each small code change. _Exact scheme TBD._

Standalone this is jac313 minus the umbrella: `Qlite::v002` ↔ `jac313::Qlite::v002`.

## Quick start

```cpp
#include <Qlite/v002.hpp>

Qlite::v002::Sqlite db("mydata.db");
db.exec("CREATE TABLE IF NOT EXISTS events (id INTEGER, payload TEXT)");
db.exec("INSERT INTO events VALUES (?, ?)", 1, std::string("hello"));
```

## Build

Requires SQLite3 development headers (`find_package(SQLite3)`) and a **C++26** compiler
(gcc-toolset-15 / GCC 15.2; clang 21 — see [Cpp26_Adoption.md](../docs/Cpp26_Adoption.md)).

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
./build/examples/qlite_v002_example
```

## Implementation layout (single source of truth)

The wrapper has **one implementation**, consumed two ways — a textual header and an
optional C++ module both paste the *same* body; they differ only in how `std` and
`<sqlite3.h>` names are made visible to it.

| Path | Purpose |
|------|---------|
| `include/Qlite/v002/Sqlite.body.hpp` | The implementation **body** — an include-free fragment (no guard, includes nothing). Must be `#include`d *inside* the `Qlite::v002` namespace. |
| `include/Qlite/v002/Sqlite.hpp` | Textual header for non-module consumers: textual `std` + `<sqlite3.h>` includes, then the body inside the namespace. |
| `include/Qlite/v002.hpp` | Public entry point (`#include`s `Sqlite.hpp`). Spell `<Qlite/v002.hpp>` in your code. |
| `modules/Qlite.v002.cppm` | C++ module interface: `<sqlite3.h>` in the global module fragment, the body inside `export namespace`. See [Modules](#modules) below. |
| `examples/v002_example.cpp` | Minimal usage demo. |
| `tests/v002_test.cpp` | Header unit test. |
| `tests/module/v002_module_test.cpp` | Functional module test (round-trips an in-memory DB through the imported module). |

## The four features

| # | Feature | API | Doc |
|---|---------|-----|-----|
| 1 | **RAII transaction guard** | `auto tx = db.transaction(); … tx.commit();` else auto-rollback on scope exit | [transactions.md](../docs/qlite/transactions.md) |
| 2 | **Prepared-statement cache** | `db.prepare_cached(sql) -> Statement&` (reusable, cached) | [persistence.md](../docs/qlite/persistence.md) |
| 3 | **Ergonomics** | `string_view` / `std::span<const std::byte>` (blob) binds; `column_index` / `get_by_name` / `column_count` / `column_name`; `bind_param(":name", v)`; `get_all<Ts...>()` | [binding.md](../docs/qlite/binding.md) |
| 4 | **`std::expected` path** | `try_exec` / `try_step` (guarded by `__cpp_lib_expected`), alongside the throwing API | [errors.md](../docs/qlite/errors.md) |

All four are **additive**. The frozen surface the jac313 Store SQL sink depends on
(`Sqlite` ctor, `exec`, `exec(sql, args...)`, `begin`/`commit`/`rollback`, `prepare`;
`Statement`'s `bind`, `step`, `reset`, `clear_bindings`, `bind_int64`, `bind_double`,
`get`, `get_row`; `column<T>`, `bind_value`, `SqliteError::code()`, `version()`) stays
intact.

### C++26 contracts

In v002 the wrapper's invariants are guarded by the `contracts` pre/post/assert shim
(`QLITE_PRE` / `QLITE_POST` / `QLITE_ASSERT`, runtime checks today, migrating to native
P2900 contracts once `__cpp_contracts` is available). See
[Cpp26_Adoption.md](../docs/Cpp26_Adoption.md).

## Modules

```bash
cmake -G Ninja -S . -B build-mod -DQLITE_BUILD_MODULES=ON
cmake --build build-mod
import Qlite.v002;   // exposes Qlite::v002::{Sqlite, …}
```

Module builds require the Ninja generator. The module exports the same public API,
qualified under `Qlite::v002`, as the textual header. The `import std;` story is in
[docs/Modules.md](../docs/Modules.md).

## CMake targets

- `Qlite::Qlite` — INTERFACE library (headers + link to `SQLite::SQLite3`).
- `Qlite::Qlite_module` — C++ module object library (when `QLITE_BUILD_MODULES=ON`).

_Exact target names / option spellings: TBD — owned by the build files._
