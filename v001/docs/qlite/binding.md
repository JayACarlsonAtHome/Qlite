# Binding and extraction — v001

How values go *into* statements (binds) and come back *out* (column extraction), and the
ergonomic conveniences layered on top.

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
no other changes. Both are part of the frozen surface.

Both are free functions in `Sqlite.body.hpp`:

```cpp
template <class T> void bind_value(sqlite3_stmt* st, int idx, T v);  // one overload per bound type
template <class T> T    column(sqlite3_stmt* st, int col);           // one specialization per read type
```

**Types wired out of the box:**

| Type | `bind_value` (in) | `column<T>` (out) |
|------|:-----------------:|:-----------------:|
| `std::int64_t` | yes | yes |
| `int` | yes | yes |
| `double` | yes | yes |
| `std::string` | yes | yes |
| `std::string_view` / `const char*` | yes | — (read as `std::string`) |
| blob: `std::span<const std::byte>` in / `std::vector<std::byte>` out | yes | yes |
| `std::optional<T>` (SQL NULL ⇄ `std::nullopt`) | yes | yes (int64 / double / string) |

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
st.bind(1, std::span<const std::byte>{buf});            // blob
st.bind_param(":id", 42);                               // named parameter
auto rows = db.prepare("SELECT id, name FROM t").get_all<int, std::string>();
int i = st.column_index("name");                        // column by name
```

Precise `Statement` extraction API (from `Sqlite.body.hpp`):

```cpp
template <class... Ts> void                            get(Ts&... out);      // fill refs, left to right
template <class... Ts> std::tuple<Ts...>              get_row();             // current row as a tuple
template <class... Ts> std::vector<std::tuple<Ts...>> get_all();             // every remaining row
template <class T>      T                             get_by_name(std::string_view col);
int                                                   column_index(std::string_view col);  // throws if absent
int                                                   column_count() const;
std::string                                           column_name(int i) const;
```

Each of `get` / `get_row` / `get_all` / `get_by_name` routes through `column<T>`, so every
type in the table above works in all of them (e.g. `get_all<int, std::string>()`).

---

## Scalar `get_one_*` helpers

Additive helpers in `Sqlite.body.hpp` for single-value queries (not part of the frozen
Store SQL surface):

| Helper | Behaviour |
|--------|-----------|
| `get_one_long` / `get_one_string` / `get_one_double` | Variadic binds; no row → type default (0 / `""` / 0.0) |
| `get_one_long_or` / `get_one_string_or` | Explicit default when no row matches |
| `get_one_*_debug` or `debug=true` overload | Logs SQL and result to **stderr via `fprintf`** (not `std::cerr`) |

SQL/prepare errors propagate — a malformed query must not silently read as a clean zero.

---

## How the pieces fit

Binding feeds a `Statement` (see [persistence.md](persistence.md)); extraction reads it
back after `step()`. The non-throwing twins (`try_exec` / `try_step`) carry the same bind
semantics but return `std::expected` instead of throwing — see [errors.md](errors.md).
