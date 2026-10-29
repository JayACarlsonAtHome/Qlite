// File:    docs/Cpp26_Adoption.md
// Purpose: What C++26 is available on our toolchains, what Qlite v002 has adopted, and the
//          plan for the rest; contracts are native on GCC 16 (see Contracts).

# C++26 Adoption — Qlite v002

v002's build baseline is **`cxx_std_26`**. The wrapper compiles clean under `-std=c++26`
on the toolchains below; this page records *what the compilers actually support*, what
v002 has adopted, and what's deferred. v002 is the **frontier** sibling of the
[v001 C++23 baseline](../../v001/README.md) — see [Architecture.md](Architecture.md) for
why the worlds are duplicated so v002 can advance independently.

## Toolchains & feature availability

> Detect via `g++ -std=c++26 -dM -E -x c++ <(echo '#include <version>')` and direct
> compiles — **not** from memory. Fill the table once measured for this repo.

| Toolchain | C++26 (`-std=c++26`) | Notes |
|---|---|---|
| system g++ (old) | ❌ | too old — not used for v002 |
| **gcc-toolset-15 (GCC 15.2.x)** | ✅ (intended) | the standard build compiler — _confirm_ |
| **clang 21.x** | ⚠️ (intended) | second gate — _confirm; record any workaround in [findings/](findings/README.md)_ |
| **GCC 16.2.1** (Fedora 44) | ✅ | `__cpp_contracts 202502L` at `-std=c++26` (measured 2026-10-29): native contracts |
| clang 22.1.8 (Fedora 44) | ✅ | no `__cpp_contracts` (measured 2026-10-29) |

_Exact `__cplusplus` values and the available feature-test macros: **_TBD_** — record from
a real `-dM` dump, not from memory._

## Contracts (adopted)

`Qlite/include/Qlite/v002/contracts.hpp` gives `QLITE_PRE` / `QLITE_POST` / `QLITE_ASSERT`,
one spelling for every compiler. Database.hpp (and the module) includes it before the bodies.

| Compiler | What the macros are |
|---|---|
| **GCC 16** (`-std=c++26`) | native C++26 `contract_assert` (P2900). This world's CMakeLists.txt adds `-fcontracts -fcontract-evaluation-semantic=enforce` (compile and link): a violation terminates. GCC 16 turns contracts on at C++26 by itself, but links the violation handler only with `-fcontracts`, so any consumer building these headers at C++26 with GCC 16 links with it. |
| clang (22 has no `__cpp_contracts`), GCC 15, any C++23 build | a runtime check that prints the condition and where, then aborts |
| `-DQLITE_CONTRACTS_ENABLED=0` | nothing |

What is checked (programmer errors; what the database reports stays an `Error`):

- **Reading a column needs a current row** (`get`, `get_row`, `get_at`, `get_by_name`), and a
  column inside it. Before `step()`, or after it returned false, SQLite and PostgreSQL read
  as NULL / 0 without a word, and MariaDB handed back the previous row.
- **SQLite binds name placeholders:** `bind(a, b, ...)` takes no more values than the SQL has
  `?`s, and `bind_int64` / `bind_double` / `bind_text` / `bind_blob` / `bind_null` take an
  index from 1 to that count. SQLite returns `SQLITE_RANGE` for these, which went unchecked,
  so the value was dropped. (PostgreSQL and MariaDB already throw for a count that does not match.)

Test: `qlite_v002_contracts_test` (SQLite; each misuse terminates, the right use does not).
The checks sit in the function body, not on the declarator: a macro cannot put `pre()` /
`post()` there portably.

## What C++26 buys Qlite

The lens for adoption is **safety** (memory / overflow / contract). Candidate adoptions
for the wrapper — mark each as done/not-yet as the code lands:

| C++26 feature | Where it could help Qlite |
|---------------|---------------------------|
| **contracts** | done: a current row for column reads, SQLite bind counts and indices (see Contracts) |
| **`std::span::at` / bounds** | bounds-checked blob reads on the column side |
| **saturating arithmetic** | overflow-safe size/offset math for blob and `get_all` buffers |
| **`= delete("reason")`** | clearer diagnostics on deleted copy of RAII types (`Sqlite`, `Statement`, the transaction guard) |
| **pack indexing / `_` placeholders** | the variadic bind/column "peel" code ([binding.md](qlite/binding.md)) |

_Status of each: **_TBD_** — to be filled as adoptions land with regression tests._

## Toolchain finding (placeholder)

Going to two standards in one real codebase surfaces compiler differences that hello-world
never reaches. If a C++26-only divergence shows up here (e.g. a `std::print` /
constant-eval / modules defect), it gets isolated to a minimal reproducer in
[findings/](findings/README.md) and written up here with a compiler accept/reject table
and the workaround v002 ships.

_No finding recorded yet — ready to fill today._

## Building with C++26

```bash
# RHEL: activate the toolset first (system g++ is too old)
source /opt/rh/gcc-toolset-15/enable
cmake -G Ninja -S . -B build-gcc15 -DCMAKE_CXX_COMPILER=g++
cmake --build build-gcc15 && ctest --test-dir build-gcc15
# clang is the second gate (clang 21 supports -std=c++26)
```

_Exact CMake options and the `cxx_std_26` wiring: TBD — owned by the build files._
