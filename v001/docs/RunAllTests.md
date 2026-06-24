//File:    docs/RunAllTests.md
//Purpose: One-stop runbook — the full Qlite v001 test battery via Qlite_test_cli

# RunAllTests — the full Qlite v001 test battery

The runbook for exercising the whole v001 world: build and run every relevant combination
of **{compiler} × {build_type} × {modules}**, then check results. The same compiled runner
(`Qlite_test_cli`) is the orchestrator — no GitHub Actions, no shell-script CI.

> **Anti-drift:** this is the *same* runner and suite that runs inside jac313 against the
> vendored copy. A green run in both places proves the embedded copy hasn't drifted. See
> [Architecture.md](Architecture.md#the-forward-port-to-jac313-story).

See also: [Setup.md](Setup.md) · [README.md](../README.md) · [Results.md](Results.md)

---

## 0. Prerequisites (one-time)

```bash
./bootstrap.sh        # senses toolchain, builds the runner, hands off
```

Run everything **from inside `v001/`**. Set the toolchain activation once per shell (RHEL
needs the toolset activated; Mint has `g++-15` / `clang` on `PATH`):

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
| compiler | g++-15, clang |
| build_type | Debug, Release |
| modules | off, on |

_Exact subcommands, flags, and the per-axis values: TBD — owned by the runner._

---

## 2. The smaller loops

```bash
# header-only unit test, single config
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

_Which gates are wired into the runner: TBD._

---

## 4. Reading the output

Numbers and pass/fail land per the runner's reporting. Before trusting any throughput
number, read [HowToReadResults.md](HowToReadResults.md); recorded results live in
[Results.md](Results.md) (_pending first matrix run_).
