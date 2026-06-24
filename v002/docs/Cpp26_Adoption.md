// File:    docs/Cpp26_Adoption.md
// Purpose: What C++26 is available on our toolchains, what Qlite v002 has adopted, and the
//          migration plan for features that aren't compiler-ready yet (contracts).

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
| gcc-toolset-16 / GCC 16 | _TBD_ | would add native contracts / reflection |

_Exact `__cplusplus` values and the available feature-test macros: **_TBD_** — record from
a real `-dM` dump, not from memory._

## The `contracts` shim (adopted)

ISO **contracts** (P2900) are **not available yet** on the intended gates — no
`__cpp_contracts`; the native `pre()/post()/contract_assert()` syntax errors on GCC 15.2 /
clang 21. v002 adopts a **`contracts` shim** so the wrapper can express its invariants
today and migrate mechanically later:

```cpp
// Qlite/Setup contracts shim — runtime fail-fast checks today
QLITE_PRE(idx >= 1);                 // precondition
QLITE_POST(rc == SQLITE_OK);         // postcondition
QLITE_ASSERT(stmt != nullptr);       // mid-function assertion
```

| Shim macro | Today (runtime) | Native C++26 (when `__cpp_contracts`) |
|------------|-----------------|----------------------------------------|
| `QLITE_PRE(cond)` | runtime fail-fast check | `pre(cond)` on the function declarator |
| `QLITE_POST(cond)` | runtime fail-fast check | `post(r: cond)` on the function declarator |
| `QLITE_ASSERT(cond)` | runtime fail-fast check | `contract_assert(cond)` |

_Exact macro names and the shim header path are **_TBD_** — owned by the Setup component
(mirrors jac313's `contracts.hpp` pattern)._

## What C++26 buys Qlite

The lens for adoption is **safety** (memory / overflow / contract). Candidate adoptions
for the wrapper — mark each as done/not-yet as the code lands:

| C++26 feature | Where it could help Qlite |
|---------------|---------------------------|
| **contracts (via shim)** | pre/post on `bind_*` index ranges, `step`/`reset` statement state, transaction-guard invariants |
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

## Migration plan — native contracts

When a contracts-capable compiler is available (`__cpp_contracts` defined):

- `QLITE_PRE(cond)` / `QLITE_POST(cond)` → native `pre(cond)` / `post(r: cond)` on the
  function declarator.
- `QLITE_ASSERT(cond)` → `contract_assert(cond)`.

The shim header is the **single place to flip**; call sites are written to port
mechanically.

## Building with C++26

```bash
# RHEL: activate the toolset first (system g++ is too old)
source /opt/rh/gcc-toolset-15/enable
cmake -G Ninja -S . -B build-gcc15 -DCMAKE_CXX_COMPILER=g++
cmake --build build-gcc15 && ctest --test-dir build-gcc15
# clang is the second gate (clang 21 supports -std=c++26)
```

_Exact CMake options and the `cxx_std_26` wiring: TBD — owned by the build files._
