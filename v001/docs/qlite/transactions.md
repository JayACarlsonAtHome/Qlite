# Transactions — v001

The transaction API, and the **RAII transaction guard** that kills the forgot-to-rollback
bug class (feature #1).

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
| Is the guard movable? | _TBD_ — typically move-only, non-copyable. |
| Double commit / commit-after-rollback | _TBD_ — define whether it's a no-op or an error. |
| Nested transactions / savepoints | _TBD_ — whether `transaction()` nests via `SAVEPOINT`. |
| Exceptions during the destructor's rollback | _TBD_ — destructors shouldn't throw; document the swallow/log behavior. |

_These are owned by the implementation; fill them against `Sqlite.body.hpp`._

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
