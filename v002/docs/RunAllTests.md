//File:    docs/RunAllTests.md
//Purpose: One-stop runbook — the full Qlite v002 test battery via Qlite_test_cli

# RunAllTests — the full Qlite v002 test battery

The runbook for exercising the whole v002 world: build and run every relevant combination
of **{compiler} × {build_type} × {modules}**, then check results. The same compiled runner
(`Qlite_test_cli`) is the orchestrator — no GitHub Actions, no shell-script CI. v002 is the
**C++26** world (see [Cpp26_Adoption.md](Cpp26_Adoption.md)).

> **Anti-drift:** this is the *same* runner and suite that runs inside jac313 against the
> vendored copy. A green run in both places proves the embedded copy hasn't drifted. See
> [Architecture.md](Architecture.md#the-forward-port-to-jac313-story).

See also: [Setup.md](Setup.md) · [README.md](../README.md) · [Results.md](Results.md)

---

## 0. Prerequisites (one-time)

```bash
./bootstrap.sh        # senses toolchain, builds the runner, hands off
```

Run everything **from inside `v002/`**. Set the toolchain activation once per shell (RHEL
needs the toolset activated for C++26; Mint has the compilers on `PATH`):

```bash
ACT="scl enable gcc-toolset-15 --"   # Linux Mint: ACT=""
CLI="_TBD — runner path_"            # e.g. ./build-bootstrap/.../Qlite_test_cli
```

---

## 1. Run everything

```bash
$ACT $CLI <run-all subcommand>       # _TBD — exact subcommand_
```

The intent: build + run every *missing* combination for this host, idempotent and
resumable (re-run and it only does what's still missing).

| Axis | Values (intended) |
|------|-------------------|
| compiler | gcc-toolset-15, clang 21 |
| build_type | Debug, Release |
| modules | off, on |

_Exact subcommands, flags, and the per-axis values: TBD — owned by the runner._

---

## 2. The smaller loops

```bash
# header-only unit test, single config (C++26)
cmake -S . -B build && cmake --build build && ctest --test-dir build

# module functional test (Ninja)
cmake -G Ninja -S . -B build-mod -DQLITE_BUILD_MODULES=ON
cmake --build build-mod && ctest --test-dir build-mod
```

---

## 3. Optional gates

- **valgrind** (memcheck) over the wrapper — see
  [Memory-And-Concurrency.md](Memory-And-Concurrency.md).
- **Module + `import std;`** path — see [Modules.md](Modules.md).
- **C++26 contracts** — the shim's runtime checks run as ordinary assertions in the suite;
  see [Cpp26_Adoption.md](Cpp26_Adoption.md).

_Which gates are wired into the runner: TBD._

---

## 4. Reading the output

Numbers and pass/fail land per the runner's reporting. Before trusting any throughput
number, read [HowToReadResults.md](HowToReadResults.md); recorded results live in
[Results.md](Results.md) (_pending first matrix run_).
