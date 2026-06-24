# Qlite v001 — the C++23 world

A complete, self-contained **C++23** world for the `Qlite` SQLite3 wrapper. Everything in
this directory is anchored to the v001 root: the wrapper, its toolchain sensing, the docs,
and (eventually) the test runner. Changing v001 only tests v001.

> **Self-contained by design.** v001 and [v002](../v002/README.md) are fully duplicated —
> DRY is *not* applied across versions, so each can move its toolchain and language
> standard independently. The cost (one shared-infra fix applied to each world before they
> diverge) buys complete isolation. See [Architecture.md](docs/Architecture.md).

---

## Components

| Component | What it is | Namespace |
|-----------|------------|-----------|
| [**Qlite**](Qlite/README.md) | header-only SQLite3 wrapper — RAII connection + variadic bind/column over `sqlite3`, plus the four headline features | `Qlite::v001` |
| [**Setup**](Setup/README.md) | data-driven, activation-aware toolchain sensing | `Setup::v001` |

Standalone these are jac313 minus the `jac313::` umbrella:
`Qlite::v001` ↔ `jac313::Qlite::v001`, `Setup::v001` ↔ `jac313::Setup::v001`.

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

---

## Quick start

```cpp
#include <Qlite/v001.hpp>

Qlite::v001::Sqlite db("app.db");
db.exec("CREATE TABLE IF NOT EXISTS events (id INTEGER, payload TEXT)");
db.exec("INSERT INTO events VALUES (?, ?)", 1, std::string("hello"));
```

```bash
./bootstrap.sh          # sense toolchain → build the test runner → hand off
```

Requires **C++23** and system SQLite3 development headers. Full prerequisites, building,
and the test runbook are in [docs/Setup.md](docs/Setup.md) and
[docs/RunAllTests.md](docs/RunAllTests.md).

---

## Documentation

| Doc | Contents |
|-----|----------|
| [docs/Architecture.md](docs/Architecture.md) | Layout, the umbrella/version-world model, the standalone+embed philosophy, the forward-port-to-jac313 story |
| [docs/Setup.md](docs/Setup.md) | Toolchain, bootstrapping, building, running tests |
| [docs/RunAllTests.md](docs/RunAllTests.md) | The test runbook |
| [docs/Modules.md](docs/Modules.md) | C++23 modules + the `import std;` story for v001 |
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
