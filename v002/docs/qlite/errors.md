# Error handling — v002

The two error paths: the **throwing** API (`SqliteError`) and the additive **`std::expected`
non-throwing path** (feature #4). v002 is the **C++26** world, so a third, complementary
layer sits underneath both: the **contracts shim** catches *programmer* errors (out-of-range
binds, misused statement state) as fail-fast assertions, distinct from *runtime* SQLite
failures. See [Cpp26_Adoption.md](../Cpp26_Adoption.md).

See also: [persistence.md](persistence.md) · [transactions.md](transactions.md) ·
[binding.md](binding.md).

---

## Three layers, three jobs

| Layer | Catches | Mechanism |
|-------|---------|-----------|
| **Contracts shim** (`QLITE_PRE/POST/ASSERT`) | programmer misuse (bad index, wrong state) | runtime fail-fast today; native `pre`/`post`/`contract_assert` when `__cpp_contracts` lands |
| **Throwing API** (`SqliteError`) | runtime SQLite failures | C++ exceptions, `code()` carries the result code |
| **`std::expected` path** (`try_*`) | runtime SQLite failures, without exceptions | `std::expected<…, SqliteError>` |

The contracts layer is about "you called it wrong"; the other two are about "SQLite said
no." They don't overlap.

---

## The throwing path (`SqliteError`)

By default the API throws on failure. `SqliteError` carries the underlying SQLite result
code via `code()` — part of the frozen surface:

```cpp
try {
    db.exec("INSERT INTO t VALUES (?)", 1);
} catch (const Qlite::v002::SqliteError& e) {
    int rc = e.code();          // the sqlite3 result code (e.g. SQLITE_CONSTRAINT)
    // e.what() — human-readable message  (_exact text: TBD_)
}
```

---

## The `std::expected` non-throwing path (feature #4)

Guarded by `__cpp_lib_expected`, the wrapper offers non-throwing twins that return
`std::expected<T, SqliteError>` (or similar) instead of throwing:

```cpp
#if defined(__cpp_lib_expected)
auto r = db.try_exec("INSERT INTO t VALUES (?)", 1);
if (!r) {
    int rc = r.error().code();      // the error, as a value — no exception thrown
} else {
    // success — *r is the result (e.g. rows affected)  (_exact value type: TBD_)
}
#endif
```

| Throwing | Non-throwing twin |
|----------|-------------------|
| `exec(...)` | `try_exec(...)` → `std::expected<…, SqliteError>` |
| `step()` | `try_step()` → `std::expected<…, SqliteError>` |

The two paths share the same bind/column semantics ([binding.md](binding.md)); they differ
only in how failure is reported. Where `__cpp_lib_expected` is unavailable, the `try_*`
overloads compile out and only the throwing API is present.

---

## Choosing a path

| Use… | When |
|------|------|
| throwing API | normal control flow; failures are exceptional; you want stack unwinding + RAII cleanup to handle the error path |
| `try_*` (expected) | hot loops where exceptions are unwanted; you want to branch on the result code inline; `noexcept` call sites |

Both interoperate with the RAII transaction guard: on the throwing path the guard
auto-rolls-back during unwinding; on the `try_*` path you inspect the `expected` and decide
whether to `commit()` — see [transactions.md](transactions.md). A `QLITE_PRE` violation is
neither — it signals a bug to fix, not a condition to handle.

_Exact return value types of the `try_*` overloads and the full error-reporting contract:
**_TBD_** — owned by `Sqlite.body.hpp`._
