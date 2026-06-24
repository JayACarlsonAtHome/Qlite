# Setup::v002

Standalone, data-driven, **activation-aware** toolchain sensing for the
[Qlite v002 world](../README.md) (`Setup::v002`).

Most build tooling assumes a compiler is a plain binary on `PATH`. That breaks on Red
Hat, where gcc-toolset compilers need an **activation launcher** that differs by OS
(RHEL 9 `scl`, RHEL 10 `gcc-toolset-NN-env`, Ubuntu/Mint direct). `Setup` models every
mechanism as a **command template** (`{CMD}` placeholder) in data, version-sniffs
`--version` (so it's terminology-independent), and captures the activation's
**environment delta** so consumers can apply it to their own build/run children.

For v002 the sensed toolchains must support **C++26** (gcc-toolset-15 / GCC 15.2, clang
21) — see [Cpp26_Adoption.md](../docs/Cpp26_Adoption.md). Adding a new platform is a config
line in `compilers.conf`, not code.

This package is **self-contained** — its own command-runner, no dependencies beyond the
C++26 standard library — so it builds first and is consumed by the rest of the world
(e.g. the `Qlite_test_cli` runner), as a library or via the module.

Standalone this is jac313 minus the umbrella: `Setup::v002` ↔ `jac313::Setup::v002`.

```cpp
#include <Qlite/Setup/v002.hpp>          // or: import Qlite.setup.v002;
using namespace Setup::v002;

scrub_exported_shell_functions();
for (const ResolvedToolchain& tc : resolve_registry(source_dir)) {
    if (tc.available)
        // tc.cc_path  -> CMAKE_CXX_COMPILER
        // tc.env_delta -> apply_env_delta(tc.env_delta) before build + run
        ;
}
```

> API names above mirror the jac313 `Setup` surface and are indicative; the exact
> standalone signatures are **_TBD_** (owned by the headers).

## Layout

| Path | Purpose |
|------|---------|
| `include/Qlite/Setup/v002.hpp` | Public API (toolchain model, sensing, env helpers, runner) |
| `src/v002.cpp` | Implementation incl. the self-contained `run_capture` |
| `modules/Qlite.setup.v002.cppm` | C++ module re-export |
| `compilers.conf` | Tracked default registry (per-platform activation templates) |

A machine-local `compilers.local.conf` (gitignored) overrides the tracked default.

## Build options

| Option | Default | Purpose |
|--------|---------|---------|
| `QLITE_SETUP_BUILD_MODULES` | OFF | Build the `Qlite.setup.v002` module (Ninja only) |
| `QLITE_SETUP_BUILD_TESTS` | OFF | Build and register unit tests |

_Exact option spellings: TBD — owned by the build files._
