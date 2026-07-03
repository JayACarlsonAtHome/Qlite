# Transactions — v002

The transaction API, and the **RAII transaction guard** that kills the forgot-to-rollback
bug class (feature #1). v002 is the **C++26** world — the guard's state invariants are
guarded by the contracts shim (see [Cpp26_Adoption.md](../Cpp26_Adoption.md)).

See also: [persistence.md](persistence.md) · [binding.md](binding.md) ·
[errors.md](errors.md).

---

## The frozen primitives

The low-level surface the jac313 Store SQL sink depends on, unchanged:

```cpp
db.begin();      // BEGIN
db.commit();     // COMMIT
db.rollback();   // ROLLBACK
```

These work, and keep working. The problem they leave open is the classic one: an exception
(or early `return`) between `begin()` and `commit()` leaks an open transaction unless every
path remembers to `rollback()`.

---

## The RAII transaction guard (feature #1)

`db.transaction()` returns a guard that owns the transaction's lifetime:

```cpp
{
    auto tx = db.transaction();          // BEGIN
    db.exec("INSERT INTO t VALUES (?)", 1);
    db.exec("INSERT INTO t VALUES (?)", 2);
    tx.commit();                         // COMMIT — explicit success
}                                        // if commit() was NOT called: auto ROLLBACK here
```

The rule:

- **`tx.commit()` called** → the transaction commits; the guard's destructor does nothing.
- **scope exits without `commit()`** — normal return, early return, or a thrown exception
  → the destructor **rolls back automatically**.

So the only way to commit is to *say so*; every other exit unwinds cleanly. That removes
the "forgot to roll back on the error path" bug by construction.

---

## Semantics to pin down

| Question | Intended answer |
|----------|-----------------|
| Is the guard movable? | **Yes** — move-only; copy ctor/assign are `= delete`. |
| Double commit / commit-after-rollback | **No-op** — same as v001; `Transaction` has no `QLITE_PRE` checks on `commit()`/`rollback()`. |
| Nested transactions / savepoints | **No** — flat `BEGIN TRANSACTION` only; no `SAVEPOINT` nesting. |
| Exceptions during the destructor's rollback | **Swallowed** — `~Transaction()` wraps `rollback()` in `try { … } catch (...) {}`; no logging. |

_Filled against `Sqlite.body.hpp`. The contracts shim guards bind/step indices elsewhere; the
transaction guard's state machine is pointer-nulling, not `QLITE_PRE`-checked today._

---

## Combining with the statement cache

A common pattern: open a guard, reuse a cached statement in a loop, then commit once.

```cpp
auto tx = db.transaction();
auto& st = db.prepare_cached("INSERT INTO t VALUES (?)");   // see persistence.md
for (int i = 0; i < n; ++i) { st.reset(); st.bind(1, i); st.step(); }
tx.commit();
```

Batching writes inside one transaction is also the difference between a fast and a slow
durable rate — see [HowToReadResults.md](../HowToReadResults.md).
