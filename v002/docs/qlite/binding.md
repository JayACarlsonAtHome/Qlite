# Binding and extraction — v002

How values go *into* statements (binds) and come back *out* (column extraction), and the
ergonomic conveniences layered on top. v002 is the **C++26** world — the bind index ranges
and statement state are guarded by the contracts shim (see
[Cpp26_Adoption.md](../Cpp26_Adoption.md)).

See also: [persistence.md](persistence.md) (statements + cache) ·
[transactions.md](transactions.md) · [errors.md](errors.md).

---

## The variadic "peel first, recurse on ellipses" pattern

`exec(sql, args...)` and the row helpers bind a parameter pack by **peeling the first
argument, binding it at the current 1-based index, then recursing on the rest**:

```cpp
// conceptual shape
void bind_all(Statement& st, int idx) {}                 // base: nothing left
template <class T, class... Rest>
void bind_all(Statement& st, int idx, T&& v, Rest&&... rest) {
    bind_value(st, idx, std::forward<T>(v));             // peel + bind one
    bind_all(st, idx + 1, std::forward<Rest>(rest)...);  // recurse on the ellipses
}
```

The single customization seam is **`bind_value`** (in) and **`column<T>`** (out): support
a new type by specializing those, and the variadic machinery picks it up everywhere with
no other changes. Both are part of the frozen surface. (C++26 **pack indexing** is a
candidate to simplify this code — see [Cpp26_Adoption.md](../Cpp26_Adoption.md).)

_Exact signatures: see `Sqlite.body.hpp`; summarized here, **_TBD_** for the precise
template forms._

---

## Core binds and columns (frozen surface)

| Operation | API |
|-----------|-----|
| Bind a typed value at index | `bind_value(st, i, v)` |
| Bind an integer / double directly | `bind_int64` / `bind_double` |
| Read a typed column | `column<T>(st, i)` |
| Read a whole row as a tuple | `get` / `get_row` |

These keep working unchanged — the jac313 Store SQL sink depends on them.

---

## Ergonomic binds and extraction (feature #3)

Additive conveniences on top of the core:

| Convenience | What it does |
|-------------|--------------|
| `string_view` binds | bind text without forcing a `std::string` copy |
| **blob binds** | `std::span<const std::byte>` binds a binary blob directly |
| column-by-name | `column_index(name)`, `get_by_name(name)`, plus `column_count()` / `column_name(i)` for introspection |
| named-parameter binds | `bind_param(":name", v)` binds to a `:name` placeholder instead of a positional `?` |
| whole-result extraction | `get_all<Ts...>()` pulls every row as a container of `std::tuple<Ts...>` |

```cpp
std::byte buf[4]{};
st.bind(1, std::span<const std::byte>{buf});            // blob (C++26 span::at on read)
st.bind_param(":id", 42);                               // named parameter
auto rows = db.prepare("SELECT id, name FROM t").get_all<int, std::string>();
int i = st.column_index("name");                        // column by name
```

_Exact method names/overloads are summarized from the feature spec; precise signatures are
**_TBD_** against the headers._

---

## How the pieces fit

Binding feeds a `Statement` (see [persistence.md](persistence.md)); extraction reads it
back after `step()`. The non-throwing twins (`try_exec` / `try_step`) carry the same bind
semantics but return `std::expected` instead of throwing — see [errors.md](errors.md). In
v002 a `QLITE_PRE` checks the bind index is in range before touching SQLite.
