# Setup, build, and testing — v002

Everything you need to go from a fresh box to a green test run for the v002 (C++26) world.
The [v002 README](../README.md) is the overview; this is the how-to. For the C++26 specifics
(contracts shim, toolchain feature availability), see [Cpp26_Adoption.md](Cpp26_Adoption.md).

---

## 1. Toolchain

**C++26** is required. The wrapper itself depends only on system SQLite3 development
headers; the `std::expected` non-throwing path is guarded by `__cpp_lib_expected`, and the
C++26 contracts are expressed through a shim (runtime today; native when `__cpp_contracts`
lands).

**gcc-toolset-15 / GCC 15.2** and **clang 21** are the intended C++26 gates. Module builds
need the **Ninja** generator; `import std;` needs a specific pinned CMake (see
[Modules.md](Modules.md)).

### Getting a C++26 compiler

- **RHEL / Fedora:** `sudo dnf install gcc-toolset-15` (activated via the registry —
  `gcc-toolset-15-env` on RHEL 10, `scl enable` on RHEL 9; handled by [Setup](../Setup/README.md)).
  On RHEL: `source /opt/rh/gcc-toolset-15/enable` before building.
- **clang 21** is the second gate (`-std=c++26`); record any required workaround in
  [findings/](findings/README.md) and [Cpp26_Adoption.md](Cpp26_Adoption.md).

### SQLite3

Install the SQLite3 development package so `find_package(SQLite3)` resolves:

- **RHEL / Fedora:** `sudo dnf install sqlite-devel`
- **Ubuntu / Mint:** `sudo apt install libsqlite3-dev`

_Exact minimum versions: TBD._

---

## 2. Bootstrapping

```bash
cd v002
./bootstrap.sh        # senses the host toolchain, builds the test runner, hands off
```

`bootstrap.sh` exists only because you can't run a compiled tool before you have a
compiler. It senses the host (via [Setup](../Setup/README.md)), builds the runner once,
and hands control to it. Run everything **from inside `v002/`** — build trees and results
stay anchored to the version root.

_The exact runner path and hand-off details are owned by `bootstrap.sh` and are **_TBD_**
in this doc._

---

## 3. Building

```bash
# textual (default) — C++26
cmake -S . -B build
cmake --build build

# modules (Ninja)
cmake -G Ninja -S . -B build-mod -DQLITE_BUILD_MODULES=ON
cmake --build build-mod
```

---

## 4. Testing

```bash
ctest --test-dir build
./build/examples/qlite_v002_example
```

The full runbook — every compiler × build-type × modules combination — is in
[RunAllTests.md](RunAllTests.md). The same `Qlite_test_cli` runner and suite also run
inside jac313 against the vendored copy, proving no drift (see
[Architecture.md](Architecture.md#the-forward-port-to-jac313-story)).

_Exact CMake option names and test target names: TBD — owned by the build files._
