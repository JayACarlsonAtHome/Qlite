# Qlite::v001

The **C++23** wrapper component of the [v001 world](../README.md).

`Qlite::v001` is a **header-only** SQLite3 wrapper — the canonical evolution of the legacy
`jacQlite` / `jac::qlite` header-only wrapper. It depends only on system SQLite3. Public
symbols: `Sqlite` (with `Statement`, `Transaction`, `prepare`/`prepare_cached`, `get_one_*`
helpers), `SqliteError`, `bind_value`, `column`, `version`.

`version()` returns `"major.minor"`: major = the `v001` API line, minor = revision (bump on
each small code change; breaking changes move major). Current: **`v001.005`**.

Standalone this is jac313 minus the umbrella: `Qlite::v001` ↔ `jac313::Qlite::v001`.

## Quick start

```cpp
#include <Qlite/v001.hpp>

Qlite::v001::Sqlite db("mydata.db");
db.exec("CREATE TABLE IF NOT EXISTS events (id INTEGER, payload TEXT)");
db.exec("INSERT INTO events VALUES (?, ?)", 1, std::string("hello"));
```

## Build

Requires SQLite3 development headers (`find_package(SQLite3)`) and a C++23 compiler.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
./build/examples/qlite_v001_example
```

## Implementation layout (single source of truth)

The wrapper has **one implementation**, consumed two ways — a textual header and an
optional C++23 module both paste the *same* body; they differ only in how `std` and
`<sqlite3.h>` names are made visible to it.

| Path | Purpose |
|------|---------|
| `include/Qlite/v001/Sqlite.body.hpp` | The implementation **body** — an include-free fragment (no guard, includes nothing). Must be `#include`d *inside* the `Qlite::v001` namespace. |
| `include/Qlite/v001/Sqlite.hpp` | Textual header for non-module consumers: textual `std` + `<sqlite3.h>` includes, then the body inside the namespace. |
| `include/Qlite/v001.hpp` | Public entry point (`#include`s `Sqlite.hpp`). Spell `<Qlite/v001.hpp>` in your code. |
| `modules/Qlite.v001.cppm` | C++23 module interface: `<sqlite3.h>` in the global module fragment, the body inside `export namespace`. See [Modules](#modules-c23) below. |
| `examples/v001_example.cpp` | Minimal usage demo. |
| `tests/v001_test.cpp` | Header unit test. |
| `tests/module/v001_module_test.cpp` | Functional module test (round-trips an in-memory DB through the imported module). |

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

## Modules (C++23)

```bash
cmake -G Ninja -S . -B build-mod -DQLITE_BUILD_MODULES=ON
cmake --build build-mod
import Qlite.v001;   // exposes Qlite::v001::{Sqlite, …}
```

Module builds require the Ninja generator. The module exports the same public API,
qualified under `Qlite::v001`, as the textual header. The `import std;` story, the
quirks, and the curses are in [docs/Modules.md](../docs/Modules.md).

## CMake targets

- `Qlite::Qlite` — INTERFACE library (headers + link to `SQLite::SQLite3`).
- `Qlite::Qlite_module` — C++23 module object library (when `QLITE_BUILD_MODULES=ON`).

_Exact target names / option spellings: TBD — owned by the build files._
