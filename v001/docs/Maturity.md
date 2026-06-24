# Maturity — an honest self-assessment (v001)

Where Qlite v001 actually sits, on a 12-level scale, and why. Like the rest of these docs
this is candid: the goal is an accurate read, not a flattering one. The
[v001 README](../README.md) is the overview.

## The scale

Twelve rungs in three bands — **local**, **professional**, **business**.

| # | Level | Roughly means |
|---|-------|---------------|
| 1 | Local throwaway | Scratch experiment; works once on the author's machine. No tests, no docs. |
| 2 | Local project | Runs reliably for the author; minimal structure and notes. |
| 3 | Good local project | Clean layout, a README, reproducible build, some tests. |
| 4 | Hardened local project | Solid tests, edge cases/errors handled, documented — single-author scope. |
| 5 | Start of professional work | Versioned/namespaced API, real test discipline, multi-config builds. |
| 6 | Professional work | Comprehensive tests + docs, multi-compiler/platform, deliberate design. |
| 7 | Very good professional work | Strong test *infrastructure*, memory/thread-safety gating, candid docs. |
| 8 | Hardened professional work | + fuzzing, sanitizers, security review, performance gating. |
| 9 | Start of business quality | CI/CD, versioned releases, multiple contributors, review process. |
| 10 | Business quality | Supported releases, compatibility guarantees, broad CI matrix, external consumers. |
| 11 | Very good business quality | Security/supply-chain scanning, signed/reproducible releases, governance. |
| 12 | Tier-1 business quality | The gold standard: rigorous everything, long-term support, audited process. |

## Where Qlite v001 sits

_TBD — to be filled once the test infrastructure and first matrix run exist._

The honest split to assess is **craft vs. structure**:

- **Craft** — the single-body/two-front-end design, the four additive features over a
  frozen surface, the shared-runner anti-drift discipline. _Rung: TBD._
- **Structure** — solo author, no external CI by design (the local runner *is* the CI),
  results committed into the repo. _Rung: TBD._

## What would move the needle

- **Up to professional (5–7):** the `Qlite_test_cli` matrix landing, a recorded results
  run, the valgrind gate wired in.
- **Toward hardened (8):** fuzzing the bind/column paths, ASan/UBSan alongside valgrind, a
  security pass over the SQL-facing surface.
- **Business rungs (9+):** organizational scaffolding (CI/CD, releases, contributors) —
  **deliberate non-goals** for a solo reference library.

## The honest caveats

- _TBD — fill with the real limits once the suite runs._
- Anti-drift via the shared runner is a discipline, not a substitute for external review.
