# Qlite

A modern, **header-only C++ SQLite3 wrapper**, developed and proven here in isolation,
then embedded — wholesale — into the [jac313](https://github.com/) monorepo as
`jac313::Qlite::vNNN`. Standalone, `Qlite` lives under `Qlite::vNNN`; it depends only on
system SQLite3.

> Qlite is the **canonical home** of this wrapper. It is the evolution of the legacy
> `jacQlite` / `jac::qlite` repo, which is being retired. The library is developed and
> tested standalone here, and a *copy* is forward-ported into jac313 — jac313 never
> build-depends on this repo (see [Standalone + embed](#standalone--embed-never-depend)).

---

## 🔬 It found a real toolchain bug

_TBD — to be filled._ The reason to carry a non-trivial, two-standard wrapper across
real compilers is to make toolchains misbehave where hello-world never will. When a
minimal reproducer earns its place, it lands in `vNNN/docs/findings/` and is written up
here.

| Compiler | `-std=c++23` | `-std=c++26` |
|---|---|---|
| gcc 15.x | _TBD_ | _TBD_ |
| clang 21.x | _TBD_ | _TBD_ |

See [v001/docs/findings/](v001/docs/findings/README.md) and
[v002/docs/findings/](v002/docs/findings/README.md).

---

## The two version worlds: a C++23 baseline and a C++26 frontier

Qlite mirrors jac313's shape: an umbrella over two **self-contained version worlds**.
They are **fully duplicated** — DRY is *not* applied across them — so each can move its
toolchain and language standard independently and testing stays isolated (changing v001
only tests v001).

| Version | Standard | State |
|---------|----------|-------|
| [**v001/**](v001/README.md) | **C++23** | The proven baseline — the `Qlite::v001` wrapper + `Setup::v001` toolchain sensing, textual headers and an optional C++23 module. _Status TBD._ |
| [**v002/**](v002/README.md) | **C++26** | The frontier — `Qlite::v002` + `Setup::v002`, using real C++26 (a `contracts` pre/post/assert shim, etc.). See [v002/docs/Cpp26_Adoption.md](v002/docs/Cpp26_Adoption.md). _Status TBD._ |

Each world contains the same parts: the `Qlite` wrapper component, a `Setup` component
(toolchain sensing), a `bootstrap.sh`, a `docs/` tree, and (eventually) a
`Qlite_test_cli` compiled test runner.

---

## The four headline features

Over the legacy `jacQlite` API, `Qlite` adds four features — all **additive**; the
frozen surface that downstreams depend on keeps working (see
[Frozen surface](#the-frozen-surface)).

| # | Feature | One line |
|---|---------|----------|
| 1 | **RAII transaction guard** | `auto tx = db.transaction(); … tx.commit();` else auto-rollback on scope exit — kills the forgot-to-rollback bug class. |
| 2 | **Prepared-statement cache** | `db.prepare_cached(sql)` returns a reusable cached `Statement&`. |
| 3 | **Ergonomics** | `string_view` / `std::span<const std::byte>` (blob) binds, column-by-name, named-parameter binds (`bind_param(":name", v)`), `get_all<Ts...>()`. |
| 4 | **`std::expected` non-throwing path** | `try_exec` / `try_step` (guarded by `__cpp_lib_expected`), alongside the throwing API. |

Per-feature detail lives in each world's `docs/qlite/`:
[binding](v001/docs/qlite/binding.md) ·
[transactions](v001/docs/qlite/transactions.md) ·
[persistence](v001/docs/qlite/persistence.md) ·
[errors](v001/docs/qlite/errors.md).

### The frozen surface

These symbols are what jac313's Store SQL sink depends on and must keep working:
`Sqlite` ctor, `exec`, `exec(sql, args...)`, `begin`/`commit`/`rollback`, `prepare`, and
`Statement`'s `bind`, `step`, `reset`, `clear_bindings`, `bind_int64`, `bind_double`,
`get`, `get_row`, plus `column<T>`, `bind_value`, `SqliteError::code()`, and `version()`.
The four features above never break this surface.

---

## Standalone + embed, never depend

This is the core philosophy, derived from real decisions:

1. **Standalone + embed, never depend.** Qlite is developed and proven in isolation here,
   then a *copy* is forward-ported into jac313. jac313 **never build-depends on this
   repo** — it embeds a vendored copy. Rationale: the "outside-dependency trap" — moved
   tags, missing checkouts, upstream churn — is what breaks builds. A vendored copy can't
   be broken by upstream.
2. **Self-contained version worlds.** v001 and v002 are fully duplicated; DRY is
   deliberately not applied across versions, so each can move its toolchain/standard
   independently and testing stays isolated.
3. **Bring it all in, wholesale.** When embedding into jac313, bring the complete
   self-contained world (even unused parts) — one integration event, not N. Unused-but-
   compiling code is cheap; a missing piece discovered later is expensive.
4. **Namespace rule.** Standalone is jac313 minus the `jac313::` umbrella:
   `Qlite::vNNN` ↔ `jac313::Qlite::vNNN`; `Setup::vNNN` ↔ `jac313::Setup::vNNN`. The
   forward-port simply prepends `jac313::`.
5. **Shared test runner as anti-drift.** The same `Qlite_test_cli` + same test suite runs
   in **both** this repo and inside jac313 per version, which proves the embedded copy
   hasn't drifted.

The full story is in each world's
[Architecture.md](v001/docs/Architecture.md) (umbrella/version-world model + the
forward-port-to-jac313 narrative).

---

## Quick start

```cpp
#include <Qlite/v001.hpp>

Qlite::v001::Sqlite db("app.db");
db.exec("CREATE TABLE IF NOT EXISTS events (id INTEGER, payload TEXT)");
db.exec("INSERT INTO events VALUES (?, ?)", 1, std::string("hello"));
```

```bash
cd v001
./bootstrap.sh          # sense toolchain → build the test runner → hand off
```

Always work from **inside** a version directory — build trees and results stay anchored
to the version root, which is what keeps the worlds isolated. C++23 (v001) or C++26
(v002) is required; full prerequisites are in each world's
[docs/Setup.md](v001/docs/Setup.md).

---

## Repository layout

```
Qlite/
├── README.md         # this file
├── LICENSE           # governs both versions
├── v001/             # complete C++23 world  (Qlite wrapper, Setup, docs, tests)
└── v002/             # complete C++26 world  (Qlite wrapper, Setup, docs, tests)
```

Each `vNNN/` contains:

```
vNNN/
├── README.md         # the version-world overview
├── bootstrap.sh      # sense host → build the test runner → hand off  (owned elsewhere)
├── Qlite/            # the wrapper component   (namespace Qlite::vNNN)
├── Setup/            # toolchain sensing       (namespace Setup::vNNN)
└── docs/             # this world's documentation set
```

---

## License

[LICENSE](LICENSE) governs both versions. This is reference/learning software, provided
"AS IS" without warranty; see the license for the governing terms.
