# Connection, RAII, and the statement cache — v002

The connection lifetime, how RAII owns every SQLite resource, and the **prepared-statement
cache** (feature #2). v002 is the **C++26** world — resource invariants are guarded by the
contracts shim (see [Cpp26_Adoption.md](../Cpp26_Adoption.md)).

See also: [transactions.md](transactions.md) · [binding.md](binding.md) ·
[errors.md](errors.md).

---

## The connection (`Sqlite`)

`Sqlite` is the RAII owner of a `sqlite3*` connection — part of the frozen surface:

```cpp
Qlite::v002::Sqlite db("app.db");     // opens (or creates) the database
db.exec("CREATE TABLE IF NOT EXISTS t (id INTEGER, name TEXT)");
db.exec("INSERT INTO t VALUES (?, ?)", 1, std::string("a"));
// db's destructor closes the connection
```

| Open spelling | Effect |
|---------------|--------|
| `Sqlite("app.db")` | file-backed database (created if absent) |
| `Sqlite(":memory:")` | in-memory database (fast, non-durable — see [HowToReadResults.md](../HowToReadResults.md)) |

| Default open flags | `SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE` (overridable via ctor's `flags` parameter) |
| Busy timeout | `sqlite3_busy_timeout(db_, 5000)` — 5 seconds, set immediately after open (tolerates transient locks, e.g. gcc-then-clang in `--ctest`) |
| Threading mode | Not set by Qlite — SQLite/library default |

---

## RAII all the way down

Every SQLite resource is owned by a C++ object with a destructor — no manual `sqlite3_*`
finalize/close in caller code:

| Resource | Owned by | Released when |
|----------|----------|---------------|
| `sqlite3*` connection | `Sqlite` | `Sqlite` destructs |
| `sqlite3_stmt*` | `Statement` | `Statement` destructs (or the cache is cleared) |
| an open transaction | the transaction guard | guard destructs (commit-or-rollback) — see [transactions.md](transactions.md) |

This is the safety story: scope exit cleans up, including on the exception path. The
RAII types are non-copyable (a candidate for C++26 `= delete("reason")` diagnostics).

---

## Statements: `prepare` vs `prepare_cached` (feature #2)

The frozen surface offers `prepare(sql)` — compile a statement once, drive it directly:

```cpp
auto st = db.prepare("INSERT INTO t VALUES (?, ?)");
st.bind(1, 1); st.bind(2, std::string_view{"a"}); st.step();
st.reset(); st.clear_bindings();                 // reuse for the next row
```

`prepare_cached(sql)` (feature #2) returns a **reusable cached `Statement&`** keyed by the
SQL text — the second call with the same SQL skips the compile:

```cpp
auto& st = db.prepare_cached("INSERT INTO t VALUES (?, ?)");   // compiled once, cached
// ... later, same SQL string ...
auto& again = db.prepare_cached("INSERT INTO t VALUES (?, ?)"); // cache hit — same Statement
```

| | `prepare` | `prepare_cached` |
|---|-----------|------------------|
| Returns | owning `Statement` (by value) | cached `Statement&` (reference into the cache) |
| Recompiles on repeat | yes | no — cache hit |
| Lifetime | caller's | the connection's (cache lives with `Sqlite`) |
| Reset between uses | `reset()` / `clear_bindings()` | same — the cache hands back the same statement |

No eviction — one cached `Statement` per distinct SQL string for the connection's lifetime.
The returned `Statement&` is stable across `unordered_map` rehash (node-based storage).
Invalidated by `clear_cache()` or when `Sqlite` is destroyed. An LRU cap remains a candidate
enhancement — see [UserEnhancements.md](../UserEnhancements.md).

---

## Durability note

File-backed durability is governed by SQLite's `journal_mode` and `synchronous` pragmas;
which Qlite sets (if any) by default is **_TBD_**. The throughput consequences are large —
see [HowToReadResults.md](../HowToReadResults.md).
