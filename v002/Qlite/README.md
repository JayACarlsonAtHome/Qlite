# Qlite::v002

The **C++26** wrapper component of the [v002 world](../README.md).

`Qlite::v002` is a **header-only** SQL wrapper (SQLite, PostgreSQL, MariaDB) — the canonical evolution of the legacy
`jacQlite` / `jac::qlite` header-only wrapper, built on **real C++26** (a `contracts`
pre/post/assert shim, etc. — see [Cpp26_Adoption.md](../docs/Cpp26_Adoption.md)). It
depends only on system SQLite3. Public symbols: `Sqlite` (with `Statement`, `Transaction`,
`prepare`/`prepare_cached`, `get_one_*` helpers), `SqliteError`, `bind_value`, `column`,
`version`.

`version()` returns `"major.minor"`: major = the `v002` API line, minor = revision (bump on
each small code change; breaking changes move major). Current: **`v002.011`**.

Standalone this is jac313 minus the umbrella: `Qlite::v002` ↔ `jac313::Qlite::v002`.

## One API over three databases

Qlite is SQL made light: the same API over **SQLite** (`Sqlite.hpp`), **PostgreSQL**
(`Postgres.hpp`, libpq) and **MariaDB / MySQL** (`MariaDB.hpp`, Connector/C). Each class has
the same members — `exec`, `begin`/`commit`/`rollback`, `changes()`, `prepare` /
`prepare_cached` giving a `Statement` with variadic `bind(...)`, `step()` and `get(...)`,
`Transaction` — described by the `Qlite::v002::Database` concept (`Database.hpp`), so code written
against the concept works with any of them; the `get_one_*` helpers do. Errors derive from
`Qlite::v002::Error` (`SqliteError`, `PostgresError` with its SQLSTATE, `MariaDBError` with
its error number). A `std::optional<T>` binds and reads NULL.

Values are always bound parameters, never pasted into SQL. Each database keeps its own
placeholders, so no SQL is rewritten: `?` for SQLite and MariaDB, `$1`, `$2`, ... for
PostgreSQL (a parameter's type comes from where it is used; cast where it cannot be told:
`$1::int`).

```cpp
#include <Qlite/v002/Postgres.hpp>

Qlite::v002::Postgres pg("host=/var/run/postgresql dbname=app");   // "" : the PG* environment
pg.exec("INSERT INTO events VALUES ($1, $2)", 1, std::string("hello"));

#include <Qlite/v002/MariaDB.hpp>

Qlite::v002::MariaDB maria("app");          // the local server, as the current login
auto n = Qlite::v002::get_one_long(maria, "SELECT COUNT(*) FROM events WHERE id = ?", 1);
```

Include only the databases a project uses (and link libpq / libmariadb for them);
`<Qlite/v002.hpp>` brings in SQLite. One test suite (`tests/suite.hpp`) runs on all three;
PostgreSQL and MariaDB work in a throwaway schema / database (`qlite_test_<pid>`; MariaDB
needs `` GRANT ALL ON `qlite\_test\_%`.* ``) and report skipped where no server answers.

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
| 3 | **Ergonomics** | `string_view` / `std::span<const std::byte>` (blob) binds; `column_index` / `get_by_name` / `get_at` / `column_count` / `column_name`; `bind_param(":name", v)`; `get_all<Ts...>()` | [binding.md](../docs/qlite/binding.md) |
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
qualified under `Qlite::v002`, as the textual header. Module notes and
pitfalls are in [docs/Modules.md](../docs/Modules.md).

## CMake targets

- `Qlite::Qlite` — INTERFACE library (headers + link to `SQLite::SQLite3`).
- `Qlite::Qlite_module` — C++ module object library (when `QLITE_BUILD_MODULES=ON`).

_Exact target names / option spellings: TBD — owned by the build files._
