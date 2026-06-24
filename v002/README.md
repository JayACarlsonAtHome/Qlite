# Qlite v002 — the C++26 world

A complete, self-contained **C++26** world for the `Qlite` SQLite3 wrapper — the frontier
sibling of the [v001 C++23 baseline](../v001/README.md). It uses **real C++26**: a
`contracts` pre/post/assert shim and other adoptions (see
[docs/Cpp26_Adoption.md](docs/Cpp26_Adoption.md)). Everything here is anchored to the v002
root; changing v002 only tests v002.

> **Self-contained by design.** v001 and v002 are fully duplicated — DRY is *not* applied
> across versions, so v002 can move its toolchain and language standard (to C++26)
> independently. See [Architecture.md](docs/Architecture.md).

---

## Components

| Component | What it is | Namespace |
|-----------|------------|-----------|
| [**Qlite**](Qlite/README.md) | header-only SQLite3 wrapper — RAII connection + variadic bind/column over `sqlite3`, plus the four headline features | `Qlite::v002` |
| [**Setup**](Setup/README.md) | data-driven, activation-aware toolchain sensing | `Setup::v002` |

Standalone these are jac313 minus the `jac313::` umbrella:
`Qlite::v002` ↔ `jac313::Qlite::v002`, `Setup::v002` ↔ `jac313::Setup::v002`.

---

## The four headline features

1. **RAII transaction guard** — `auto tx = db.transaction(); … tx.commit();` else
   auto-rollback on scope exit. ([transactions.md](docs/qlite/transactions.md))
2. **Prepared-statement cache** — `db.prepare_cached(sql)` returns a reusable cached
   `Statement&`. ([persistence.md](docs/qlite/persistence.md))
3. **Ergonomics** — `string_view` / blob (`std::span<const std::byte>`) binds, column-by-
   name, named-parameter binds, `get_all<Ts...>()`. ([binding.md](docs/qlite/binding.md))
4. **`std::expected` non-throwing path** — `try_exec` / `try_step`, guarded by
   `__cpp_lib_expected`. ([errors.md](docs/qlite/errors.md))

In v002 these compose with C++26 contracts: pre/post/assert checks guard the wrapper's
invariants via the `contracts` shim — see [Cpp26_Adoption.md](docs/Cpp26_Adoption.md).

---

## Quick start

```cpp
#include <Qlite/v002.hpp>

Qlite::v002::Sqlite db("app.db");
db.exec("CREATE TABLE IF NOT EXISTS events (id INTEGER, payload TEXT)");
db.exec("INSERT INTO events VALUES (?, ?)", 1, std::string("hello"));
```

```bash
./bootstrap.sh          # sense toolchain → build the test runner → hand off
```

Requires **C++26** (gcc-toolset-15 / GCC 15.2 and clang 21 are the intended gates) and
system SQLite3. Full prerequisites are in [docs/Setup.md](docs/Setup.md) and
[docs/Cpp26_Adoption.md](docs/Cpp26_Adoption.md).

---

## Documentation

| Doc | Contents |
|-----|----------|
| [docs/Architecture.md](docs/Architecture.md) | Layout, the umbrella/version-world model, the standalone+embed philosophy, the forward-port-to-jac313 story |
| [docs/Cpp26_Adoption.md](docs/Cpp26_Adoption.md) | The C++26 adoption story — the `contracts` shim, what C++26 buys, toolchain notes |
| [docs/Setup.md](docs/Setup.md) | Toolchain, bootstrapping, building, running tests |
| [docs/RunAllTests.md](docs/RunAllTests.md) | The test runbook |
| [docs/Modules.md](docs/Modules.md) | C++ modules + the `import std;` story for v002 |
| [docs/Maturity.md](docs/Maturity.md) | Honest maturity self-assessment |
| [docs/Memory-And-Concurrency.md](docs/Memory-And-Concurrency.md) | valgrind / thread-safety notes |
| [docs/HowToReadResults.md](docs/HowToReadResults.md) | How to read the result numbers |
| [docs/Results.md](docs/Results.md) | Results — _pending first matrix run_ |
| [docs/UserEnhancements.md](docs/UserEnhancements.md) | Ideas and extension points |
| [docs/Bloopers.md](docs/Bloopers.md) | The bloopers reel — owned mistakes |
| [docs/qlite/](docs/qlite/README.md) | Per-feature wrapper docs |
| [docs/findings/](docs/findings/README.md) | Minimal reproducers for toolchain bugs found |
| [docs/samples/](docs/samples/README.md) | Sample output / artifacts |

**Status:** _TBD — to be filled._
