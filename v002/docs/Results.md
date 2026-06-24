# Results — v002

Recorded results for the Qlite v002 (C++26) world.

> **Status: _pending first matrix run_.** The `Qlite_test_cli` runner has not yet recorded
> a results matrix for v002. The structure below is the intended shape; numbers land once
> the first run completes.

Read [HowToReadResults.md](HowToReadResults.md) **before** trusting any number — several
of them mislead by construction (`:memory:` vs durable, pragma-dependent rates, cache
warm/cold, the contracts-shim Debug cost). Produce these via [RunAllTests.md](RunAllTests.md).

---

## Run identity

Each recorded run is keyed by an identity so results can't drift. Intended dimensions:

| Dimension | Values (intended) |
|-----------|-------------------|
| os | _TBD_ |
| compiler | gcc-toolset-15, clang 21 |
| build_type | Debug, Release |
| modules | off, on |
| disk | ssd / hdd (auto-detected) |

_Exact identity tuple: TBD — owned by the runner._

---

## Build matrix

_TBD — pending first run._ Pass/fail per `{compiler} × {build_type} × {modules}` at C++26:

| compiler | build_type | modules | result |
|----------|------------|---------|--------|
| gcc-toolset-15 | Debug | off | _TBD_ |
| gcc-toolset-15 | Release | off | _TBD_ |
| gcc-toolset-15 | Release | on | _TBD_ |
| clang 21 | Debug | off | _TBD_ |
| clang 21 | Release | off | _TBD_ |

---

## Throughput

_TBD — pending first run._ See [HowToReadResults.md](HowToReadResults.md) for what each
column means (and the contracts-shim caveat on Debug numbers).

| config | op | N | ops/sec | db size |
|--------|----|---|---------|---------|
| _TBD_ | _TBD_ | _TBD_ | _TBD_ | _TBD_ |

---

## Cross-check vs jac313

The same suite runs against the vendored copy in jac313. When both record runs, this
section notes that they agree — the anti-drift signal. _Pending both runs._
