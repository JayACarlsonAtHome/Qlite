# Qlite::v002 — feature docs

The wrapper, documented by feature. The [component README](../../Qlite/README.md) is the
quick start and layout; these pages are the detail behind each capability. v002 is the
**C++26** world — the wrapper's invariants are guarded by the contracts shim (see
[Cpp26_Adoption.md](../Cpp26_Adoption.md)).

| Doc | Covers |
|-----|--------|
| [binding.md](binding.md) | Binding and extraction — the variadic "peel first, recurse" pattern, `bind_value` / `column<T>`, the ergonomic binds (`string_view`, blob span, column-by-name, named parameters, `get_all<Ts...>()`), scalar `get_one_*` query helpers (incl. debug/fprintf variants) |
| [transactions.md](transactions.md) | Transactions — `begin`/`commit`/`rollback` and the **RAII transaction guard** (`db.transaction()`, commit-or-auto-rollback) |
| [persistence.md](persistence.md) | The connection, RAII lifetimes, and the **prepared-statement cache** (`prepare` vs `prepare_cached`) |
| [errors.md](errors.md) | Error handling — `SqliteError` (`code()`), the throwing API, and the **`std::expected` non-throwing path** (`try_exec` / `try_step`) |

---

## The shape of the API

```cpp
#include <Qlite/v002.hpp>
using namespace Qlite::v002;

Sqlite db("app.db");                                   // RAII connection (persistence.md)
auto tx = db.transaction();                            // RAII guard      (transactions.md)
auto& st = db.prepare_cached("INSERT INTO t VALUES (?, ?)");  // cache    (persistence.md)
st.bind(1, std::string_view{"name"});                  // ergonomic bind  (binding.md)
st.step();
tx.commit();                                           // else auto-rollback on scope exit
```

The throwing API above has a non-throwing twin (`try_exec` / `try_step` returning
`std::expected`) — see [errors.md](errors.md).

Every feature here is **additive** over the frozen surface the jac313 Store SQL sink
depends on; see [Qlite/README.md](../../Qlite/README.md#the-four-features). In v002 the
methods carry contract checks (`QLITE_PRE/POST/ASSERT`) via the shim —
[Cpp26_Adoption.md](../Cpp26_Adoption.md).
