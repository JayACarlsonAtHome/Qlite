# Results — v001

Recorded results for the Qlite v001 (C++23) world.

> **Status: _pending first matrix run_.** The `Qlite_test_cli` runner has not yet recorded
> a results matrix for v001. The structure below is the intended shape; numbers land once
> the first run completes.

Read [HowToReadResults.md](HowToReadResults.md) **before** trusting any number — several
of them mislead by construction (`:memory:` vs durable, pragma-dependent rates, cache
warm/cold). Produce these via [RunAllTests.md](RunAllTests.md).

---

## Run identity

Each recorded run is keyed by an identity so results can't drift. Intended dimensions:

| Dimension | Values (intended) |
|-----------|-------------------|
| os | _TBD_ |
| compiler | g++-15, clang |
| build_type | Debug, Release |
| modules | off, on |
| disk | ssd / hdd (auto-detected) |

_Exact identity tuple: TBD — owned by the runner._

---

## Build matrix

_TBD — pending first run._ Pass/fail per `{compiler} × {build_type} × {modules}`:

| compiler | build_type | modules | result |
|----------|------------|---------|--------|
| g++-15 | Debug | off | _TBD_ |
| g++-15 | Release | off | _TBD_ |
| g++-15 | Release | on | _TBD_ |
| clang | Debug | off | _TBD_ |
| clang | Release | off | _TBD_ |

---

## Throughput

_TBD — pending first run._ See [HowToReadResults.md](HowToReadResults.md) for what each
column means.

| config | op | N | ops/sec | db size |
|--------|----|---|---------|---------|
| _TBD_ | _TBD_ | _TBD_ | _TBD_ | _TBD_ |

---

## Cross-check vs jac313

The same suite runs against the vendored copy in jac313. When both record runs, this
section notes that they agree — the anti-drift signal. _Pending both runs._
