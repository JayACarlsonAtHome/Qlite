# Samples — v001

Captured sample artifacts from running Qlite v001 — so you can see real output (schemas,
a populated database, example query results) without building and running anything.

> **Status: _TBD — to be filled._** No samples captured yet. This page describes the
> intended contents and how to regenerate them.

## Intended contents

| File | What it is | Browsable on GitHub? |
|------|------------|----------------------|
| `sample.db` | a small SQLite database the example wrote | no (binary — download + open with `sqlite3`) |
| `sample.sql` | the `CREATE` / `INSERT` statements that produced it | yes (text) |
| `sample_query.txt` | example query output read back through the wrapper | yes (text) |

_File names above are indicative; finalize when the example/runner emits them._

## Notes

- These are **captured artifacts, not live files** — they're a snapshot of one run, kept
  small by design.
- Binary `.db` files don't render on GitHub; the accompanying `.sql`/`.txt` show the same
  data as text.

## Regenerate

```bash
# build + run the example, then copy its output here
cmake -S . -B build && cmake --build build
./build/examples/qlite_v001_example
```

_Exact example path and output location: TBD — owned by the build/runner._

See [persistence.md](../qlite/persistence.md) for how the connection and statements
produce this output, and [binding.md](../qlite/binding.md) for reading rows back.
