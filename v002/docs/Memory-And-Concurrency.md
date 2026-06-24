# Memory & concurrency validation (valgrind) — v002

A record of the validation pass over the Qlite v002 wrapper using **valgrind** — memcheck
for memory safety, and helgrind + DRD for data races where concurrent use is in scope. The
[v002 README](../README.md) is the overview; this documents what was run, what it found,
what was fixed, and — honestly — what it does **not** cover. v002 is the **C++26** world,
so the contracts shim's runtime checks also act as fail-fast tripwires under these tools.

---

## Why

The wrapper owns RAII lifetimes (the connection, prepared statements, the transaction
guard) and a prepared-statement cache. memcheck answers "is every byte allocated, freed,
and accessed legally?" — exactly the question RAII and a statement cache need answered.
helgrind / DRD answer "do threads coordinate correctly?" *if* concurrent access is a
supported pattern. The C++26 contracts shim adds pre/post/assert checks that surface
violations early.

> **SQLite threading note.** SQLite itself has a threading mode (single-thread /
> multi-thread / serialized). What concurrency Qlite supports, and under which SQLite mode,
> is **_TBD_** — documented here once decided. See also [persistence.md](qlite/persistence.md).

## Method

- **valgrind** over **Debug** binaries (symbols, no optimization — the right target for
  these tools), built at **C++26**.
- Binaries driven with a **scratch working directory** so nothing in the repo is touched —
  no committed sample artifacts, no tracked DB.
- The wrapper's lifetime-heavy paths are the surface: open/close, prepare + cache reuse,
  the transaction guard's commit-vs-auto-rollback branches, bind/step/reset cycles.

```bash
# one-time
sudo dnf install valgrind            # or: apt-get install -y valgrind

# memcheck a test binary (cwd = scratch dir, so any DB file stays out of the repo)
cd /tmp/scratch
valgrind --leak-check=full --error-exitcode=1 /path/to/qlite_v002_test
```

_Exact binary paths and the runner integration: TBD._

## Findings

_TBD — to be filled. Each finding gets: what was run, what it found, the fix, and the
regression test that now guards it._

## What this does NOT cover

- Not a substitute for fuzzing or sanitizers (ASan/UBSan) — see
  [Maturity.md](Maturity.md).
- Scope and scale (which tests, which SQLite threading mode) are **_TBD_**; treat the gate
  as a tripwire, not an exhaustive sweep.
