# Findings — minimal toolchain-bug reproducers (v002)

This directory holds **minimal reproducers** for real toolchain bugs surfaced by building
Qlite v002 across compilers at **C++26** — the kind a non-trivial, header-plus-module
wrapper on the language frontier hits where hello-world never will.

> **Why this exists.** Carrying one wrapper across multiple real compilers (a textual *and*
> a module front-end, at the C++26 frontier with a contracts shim) is exactly the setup
> that makes toolchains misbehave. When a compiler or standard-library defect shows up, we
> isolate it to the smallest standalone file that still triggers it, record the diagnosis,
> and — where possible — a workaround. v002 is the most likely world to produce these,
> because it rides experimental C++26 support.

## What lands here

Each finding is a pair:

| Artifact | Purpose |
|----------|---------|
| `<name>.cpp` (or `.md`) | the minimal reproducer — compiles (or fails to) on its own, no Qlite headers needed |
| a write-up | compiler/version table, the diagnostic, the root cause, the workaround |

A finding is "done" when someone else can reproduce it from the file alone, and the
write-up says exactly which compilers accept vs reject it.

## Format for a write-up

- **One-line headline** — what breaks, on which compiler/standard (note if it's C++26-only).
- **Reproducer** — link to the `.cpp`, with the few lines that matter inline.
- **Compiler table** — `-std=c++23` vs `-std=c++26` accept/reject per compiler (the
  cross-standard split is often the whole story — see [Cpp26_Adoption.md](../Cpp26_Adoption.md)).
- **Root cause** — the actual defect (compiler vs library; constant-eval, modules, contracts).
- **Workaround** — what Qlite ships, and the trade-off it accepts.

---

_No findings recorded yet for v002 — ready to fill today._
