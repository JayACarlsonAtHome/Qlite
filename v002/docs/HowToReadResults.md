//File:    docs/HowToReadResults.md
//Purpose: How to read Qlite v002's benchmark/result numbers without fooling yourself

# How to read the results — v002

The result matrix (once recorded) reports numbers that can mislead in **both** directions.
This page is the decoder ring: what each number measures, which are real, and which are
artifacts of how the test is built. v002 is the **C++26** world — a Debug build there also
pays for the contracts shim's runtime checks (see [Cpp26_Adoption.md](Cpp26_Adoption.md)),
which is itself a thing to account for when comparing Debug vs Release.

See also: [Results.md](Results.md) (the recorded numbers) · [RunAllTests.md](RunAllTests.md)
(how to produce them) · [Architecture.md](Architecture.md) (what Qlite is).

---

## The short version

_TBD — to be filled after the first matrix run._ The intended headline shape:

- **In-memory (`:memory:`) operations** — the upper bound; no disk, no fsync.
- **Durable (on-disk) operations** — throttled by the disk and SQLite's journaling /
  synchronous mode.

Both **depend greatly on hardware** (CPU clock + disk speed). Treat any single number as a
ballpark, not a guarantee.

---

## Things that will mislead (watch for these)

| Watch for | Why it misleads |
|-----------|-----------------|
| **`:memory:` vs on-disk** | An in-memory DB flatters everything — it never touches a disk or an fsync. Don't quote it as the durable rate. |
| **`PRAGMA synchronous` / `journal_mode`** | WAL vs DELETE, `synchronous=OFF` vs `NORMAL` vs `FULL` change durable throughput by an order of magnitude. The pragma in force is part of the number's meaning. _Defaults used: TBD._ |
| **Contracts shim cost (Debug)** | v002's runtime pre/post/assert checks add work in Debug builds; a Debug-vs-Release gap is partly the contract checks, not just `-O`. |
| **Prepared-statement cache warm vs cold** | The first `prepare` compiles SQL; cached reuse skips it. A "fast" number on cached reuse isn't the cold-prepare cost. |
| **Transaction batching** | One implicit transaction per statement vs many statements in one explicit transaction differ by orders of magnitude. Which the benchmark uses changes everything. |
| **Setup-dominated tiny runs** | At small N, open/close and statement-prepare dominate; the per-op figure understates steady-state throughput. |

---

## Reading a result row

_TBD — define the columns once `Qlite_test_cli` records its first run (e.g. config /
op / N / ops-per-sec / db size)._

---

## Quick reference

| You see… | It means… | Trust it as throughput? |
|----------|-----------|--------------------------|
| ops/sec on a `:memory:` DB | in-memory ceiling | Yes, but **not** as the durable rate |
| ops/sec on an on-disk DB | depends on `synchronous`/`journal_mode` | Yes, *with the pragma context* |
| ops/sec on a tiny N | setup-dominated | No — understates steady state |
| Debug vs Release gap | partly the contracts shim, partly `-O` | Read it as both, not just optimization |

_All values pending the first recorded matrix run; see [Results.md](Results.md)._
