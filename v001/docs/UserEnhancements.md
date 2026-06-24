//File:    docs/UserEnhancements.md
//Purpose: Ideas the wrapper could grow into — captured so they aren't lost. NOT a roadmap, NOT commitments.

# User Enhancements — v001

> **Status: ideas, not promises.** Nothing here is planned, scheduled, or in progress.
> This is a notebook of "the wrapper could clearly do X" thoughts, written down so they
> survive past the conversation that produced them. For features that exist, see
> [Qlite/README.md](../Qlite/README.md) and the per-feature [docs/qlite/](qlite/README.md).

Any enhancement must respect the **frozen surface** the jac313 Store SQL sink depends on
(see [Qlite/README.md](../Qlite/README.md#the-four-features)) — additions only, never
breaks. The four headline features were all added that way; these would follow the pattern.

---

## Extension points (where the wrapper is built to grow)

| Point | The idea |
|-------|----------|
| **Bindable types** | `bind_value` / `column<T>` are the customization seam — add a specialization to support a new type (e.g. `std::chrono` time points, `std::optional<T>`, enums) without touching the core. |
| **Pragma profiles** | A small helper to apply a named pragma set (`WAL + synchronous=NORMAL`, `synchronous=OFF` for bulk import) at open time, instead of hand-written `exec("PRAGMA …")`. |
| **`get_all<Ts...>()` sinks** | Today it returns a container; a callback / generator overload (`for_each_row`) would stream large result sets without materializing them. |
| **Named-parameter ergonomics** | `bind_param(":name", v)` exists; a struct-to-params helper (bind a whole aggregate by member name) would cut boilerplate. |
| **Backup / online copy** | Wrap `sqlite3_backup_*` as an RAII `backup_to(path)` for snapshotting a live DB. |

---

## Ideas worth remembering

1. **A statement-cache eviction policy.** `prepare_cached` caches forever; an LRU cap would
   bound memory for callers that generate many distinct SQL strings.
2. **Blob streaming.** `std::span<const std::byte>` binds a whole blob; `sqlite3_blob_*`
   incremental I/O would handle blobs too large to hold in memory.
3. **A `try_*` for everything.** The `std::expected` path covers `try_exec` / `try_step`;
   extending it to `try_prepare` / `try_transaction` would make the whole API non-throwing
   end to end.
4. **Compile-time SQL checks.** With C++26 (see [v002](../../v002/README.md)), explore
   validating bind/column counts against the SQL at compile time.

_None of the above is committed. Add, prune, or promote entries as the wrapper evolves._
