// Qlite::v002 — SQLite wrapper IMPLEMENTATION BODY (single source of truth).
//
// This file is an include-fragment, NOT a standalone header: it has no include
// guard, includes nothing itself, and must be included INSIDE the `Qlite::v002`
// namespace by exactly one of two front-ends:
//
//   * Sqlite.hpp        — textual header for non-module consumers. Provides the
//                         std + <sqlite3.h> declarations via textual #include,
//                         then includes this body inside `namespace { ... }`.
//   * Qlite.v002.cppm   — module interface. Provides std via `import std;`
//                         (pilot) or textual includes, and <sqlite3.h> via the
//                         global module fragment, then includes this body inside
//                         `export namespace { ... }`.
//
// Keeping the implementation here (with no std/sqlite includes of its own) lets
// the textual header and the module share ONE definition: the only difference is
// HOW std and sqlite3 names are made visible to this body.
//
// History: the canonical evolution of the legacy `jacQlite` / `jac::qlite`
// header-only wrapper. This v002 line is the C++26 frontier (v001 is the C++23
// baseline). The public surface that downstream consumers (e.g. jac313's Store
// SQL sink) depend on is preserved; everything new here is ADDITIVE. v002 may
// additionally adopt real C++26 (e.g. the `contracts` pre/post/assert shim).

// Package version, "major.minor": major = the v002 API line, minor = the revision.
// Bump the minor on each small code change; a real (breaking) change moves the major.
[[nodiscard]] constexpr std::string_view version() noexcept { return "v002.004"; }

// ---------------------------------------------------------------------------
// Column extractors - one per return type (specialize to extend)
// ---------------------------------------------------------------------------
template<typename T>
T column(sqlite3_stmt* stmt, int col);

template<>
inline std::int64_t column<std::int64_t>(sqlite3_stmt* stmt, int col) {
    return sqlite3_column_int64(stmt, col);
}

template<>
inline int column<int>(sqlite3_stmt* stmt, int col) {
    return sqlite3_column_int(stmt, col);
}

template<>
inline double column<double>(sqlite3_stmt* stmt, int col) {
    return sqlite3_column_double(stmt, col);
}

template<>
inline std::string column<std::string>(sqlite3_stmt* stmt, int col) {
    const unsigned char* txt = sqlite3_column_text(stmt, col);
    if (!txt) return {};
    int len = sqlite3_column_bytes(stmt, col);
    return std::string(reinterpret_cast<const char*>(txt), static_cast<std::size_t>(len));
}

// Blob extraction (ergonomics): returns the raw bytes of a BLOB column.
template<>
inline std::vector<std::byte> column<std::vector<std::byte>>(sqlite3_stmt* stmt, int col) {
    const void* p = sqlite3_column_blob(stmt, col);
    int n = sqlite3_column_bytes(stmt, col);
    if (!p || n <= 0) return {};
    const std::byte* b = static_cast<const std::byte*>(p);
    return std::vector<std::byte>(b, b + static_cast<std::size_t>(n));
}

template<>
inline std::optional<std::int64_t> column<std::optional<std::int64_t>>(sqlite3_stmt* stmt, int col) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) return std::nullopt;
    return sqlite3_column_int64(stmt, col);
}

template<>
inline std::optional<double> column<std::optional<double>>(sqlite3_stmt* stmt, int col) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) return std::nullopt;
    return sqlite3_column_double(stmt, col);
}

template<>
inline std::optional<std::string> column<std::optional<std::string>>(sqlite3_stmt* stmt, int col) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) return std::nullopt;
    return column<std::string>(stmt, col);
}

// ---------------------------------------------------------------------------
// Bind value overloads - one per type (or family). The variadic Statement::bind
// peels its pack and calls bind_value() for each argument.
// ---------------------------------------------------------------------------
inline void bind_value(sqlite3_stmt* stmt, int idx, std::int64_t v) {
    sqlite3_bind_int64(stmt, idx, v);
}

inline void bind_value(sqlite3_stmt* stmt, int idx, int v) {
    sqlite3_bind_int64(stmt, idx, v);
}

inline void bind_value(sqlite3_stmt* stmt, int idx, double v) {
    sqlite3_bind_double(stmt, idx, v);
}

inline void bind_value(sqlite3_stmt* stmt, int idx, const std::string& v) {
    sqlite3_bind_text(stmt, idx, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT);
}

// string_view bind (avoids forcing a std::string copy on the hot path).
inline void bind_value(sqlite3_stmt* stmt, int idx, std::string_view v) {
    sqlite3_bind_text(stmt, idx, v.data(), static_cast<int>(v.size()), SQLITE_TRANSIENT);
}

inline void bind_value(sqlite3_stmt* stmt, int idx, const char* v) {
    if (v) sqlite3_bind_text(stmt, idx, v, -1, SQLITE_TRANSIENT);
    else   sqlite3_bind_null(stmt, idx);
}

// Blob bind (span of bytes).
inline void bind_value(sqlite3_stmt* stmt, int idx, std::span<const std::byte> v) {
    sqlite3_bind_blob(stmt, idx, v.data(), static_cast<int>(v.size()), SQLITE_TRANSIENT);
}

inline void bind_value(sqlite3_stmt* stmt, int idx, std::nullopt_t) {
    sqlite3_bind_null(stmt, idx);
}

template<typename T>
inline void bind_value(sqlite3_stmt* stmt, int idx, const std::optional<T>& v) {
    if (v.has_value()) {
        bind_value(stmt, idx, *v);
    } else {
        sqlite3_bind_null(stmt, idx);
    }
}

// ---------------------------------------------------------------------------
// Error type
// ---------------------------------------------------------------------------
class SqliteError : public std::runtime_error {
public:
    SqliteError(const std::string& msg, int errcode = SQLITE_OK)
        : std::runtime_error(msg + " (code " + std::to_string(errcode) + ")"), code_(errcode) {}
    int code() const { return code_; }
private:
    int code_;
};

// ---------------------------------------------------------------------------
// Connection (RAII)
// ---------------------------------------------------------------------------
class Sqlite {
public:
    explicit Sqlite(const std::string& filename,
                    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE) {
        if (sqlite3_open_v2(filename.c_str(), &db_, flags, nullptr) != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db_);
            sqlite3_close(db_);
            throw SqliteError("Failed to open database: " + filename + " - " + msg);
        }
    }

    ~Sqlite() {
        cache_.clear();           // finalize cached statements before closing the db
        if (db_) sqlite3_close(db_);
    }

    Sqlite(const Sqlite&) = delete;
    Sqlite& operator=(const Sqlite&) = delete;

    Sqlite(Sqlite&& other) noexcept : db_(other.db_), cache_(std::move(other.cache_)) {
        other.db_ = nullptr;
    }
    Sqlite& operator=(Sqlite&& other) noexcept {
        if (this != &other) {
            cache_.clear();
            if (db_) sqlite3_close(db_);
            db_ = other.db_;
            cache_ = std::move(other.cache_);
            other.db_ = nullptr;
        }
        return *this;
    }

    void exec(const std::string& sql) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            std::string msg = err ? err : "unknown error";
            sqlite3_free(err);
            throw SqliteError("exec failed: " + sql + " - " + msg);
        }
    }

    void begin() { exec("BEGIN TRANSACTION;"); }
    void commit() { exec("COMMIT;"); }
    void rollback() { exec("ROLLBACK;"); }

    // -----------------------------------------------------------------------
    // Prepared statement (RAII)
    // -----------------------------------------------------------------------
    class Statement {
    public:
        Statement(Sqlite& db, const std::string& sql) {
            if (sqlite3_prepare_v2(db.db_, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
                throw SqliteError("Failed to prepare: " + sql + " - " + sqlite3_errmsg(db.db_));
            }
        }

        ~Statement() {
            if (stmt_) sqlite3_finalize(stmt_);
        }

        Statement(const Statement&) = delete;
        Statement& operator=(const Statement&) = delete;

        // Movable (lets statements live in containers, e.g. the prepared-statement cache).
        Statement(Statement&& o) noexcept : stmt_(o.stmt_) { o.stmt_ = nullptr; }
        Statement& operator=(Statement&& o) noexcept {
            if (this != &o) {
                if (stmt_) sqlite3_finalize(stmt_);
                stmt_ = o.stmt_;
                o.stmt_ = nullptr;
            }
            return *this;
        }

        // Variadic positional bind: resets + clears, then binds from index 1.
        template<typename... Args>
        void bind(Args&&... args) {
            sqlite3_reset(stmt_);
            sqlite3_clear_bindings(stmt_);
            bind_impl(1, std::forward<Args>(args)...);
        }

        bool step() {
            int rc = sqlite3_step(stmt_);
            if (rc == SQLITE_ROW) return true;
            if (rc == SQLITE_DONE) return false;
            throw SqliteError("step failed", rc);
        }

        void reset() {
            sqlite3_reset(stmt_);
        }

        void clear_bindings() {
            sqlite3_clear_bindings(stmt_);
        }

        // Runtime indexed bind helpers (dynamic column counts, e.g. metric tables).
        void bind_int64(int idx, std::int64_t v) { sqlite3_bind_int64(stmt_, idx, v); }
        void bind_double(int idx, double v)       { sqlite3_bind_double(stmt_, idx, v); }
        void bind_text(int idx, std::string_view v) {
            sqlite3_bind_text(stmt_, idx, v.data(), static_cast<int>(v.size()), SQLITE_TRANSIENT);
        }
        void bind_blob(int idx, std::span<const std::byte> v) {
            sqlite3_bind_blob(stmt_, idx, v.data(), static_cast<int>(v.size()), SQLITE_TRANSIENT);
        }
        void bind_null(int idx) { sqlite3_bind_null(stmt_, idx); }

        // Named-parameter bind (":name" / "@name" / "$name" / "?NNN").
        template<typename T>
        void bind_param(const char* name, T&& v) {
            int idx = sqlite3_bind_parameter_index(stmt_, name);
            if (idx == 0) throw SqliteError(std::string("no such bind parameter: ") + name);
            bind_value(stmt_, idx, std::forward<T>(v));
        }

        // Variadic extraction using the "peel first, recurse on ellipses" pattern.
        template<typename... Ts>
        void get(Ts&... values) {
            get_impl(0, values...);
        }

        template<typename... Ts>
        std::tuple<Ts...> get_row() {
            std::tuple<Ts...> row{};
            get_tuple_impl<0>(row);
            return row;
        }

        // Collect every remaining row as a vector of tuples (ergonomics).
        template<typename... Ts>
        std::vector<std::tuple<Ts...>> get_all() {
            std::vector<std::tuple<Ts...>> out;
            while (step()) out.push_back(get_row<Ts...>());
            return out;
        }

        // Column metadata + by-name access (ergonomics).
        int column_count() const { return sqlite3_column_count(stmt_); }
        std::string column_name(int i) const {
            const char* n = sqlite3_column_name(stmt_, i);
            return n ? n : std::string{};
        }
        int column_index(std::string_view name) const {
            int n = sqlite3_column_count(stmt_);
            for (int i = 0; i < n; ++i) {
                const char* cn = sqlite3_column_name(stmt_, i);
                if (cn && name == cn) return i;
            }
            throw SqliteError("no such column: " + std::string(name));
        }
        template<typename T>
        T get_by_name(std::string_view name) {
            return column<T>(stmt_, column_index(name));
        }

#if defined(__cpp_lib_expected)
        // Non-throwing step: SQLITE errors come back as std::unexpected(SqliteError).
        std::expected<bool, SqliteError> try_step() {
            int rc = sqlite3_step(stmt_);
            if (rc == SQLITE_ROW) return true;
            if (rc == SQLITE_DONE) return false;
            return std::unexpected(SqliteError("step failed", rc));
        }
#endif

        // Escape hatch for advanced sqlite3 calls.
        sqlite3_stmt* handle() const noexcept { return stmt_; }

    private:
        void bind_impl(int /*idx*/) {} // base case

        template<typename T, typename... Rest>
        void bind_impl(int idx, T&& val, Rest&&... rest) {
            bind_value(stmt_, idx, std::forward<T>(val));
            bind_impl(idx + 1, std::forward<Rest>(rest)...);
        }

        void get_impl(int /*col*/) {} // base case

        template<typename T, typename... Rest>
        void get_impl(int col, T& val, Rest&... rest) {
            val = column<T>(stmt_, col);
            get_impl(col + 1, rest...);
        }

        template<std::size_t I, typename Tuple>
        void get_tuple_impl(Tuple& t) {
            if constexpr (I < std::tuple_size_v<Tuple>) {
                using T = std::tuple_element_t<I, Tuple>;
                std::get<I>(t) = column<T>(stmt_, static_cast<int>(I));
                get_tuple_impl<I + 1>(t);
            }
        }

        sqlite3_stmt* stmt_ = nullptr;
    };

    // -----------------------------------------------------------------------
    // RAII transaction guard: BEGIN on construction; auto-ROLLBACK on scope exit
    // UNLESS commit() (or an explicit rollback()) was called. Kills the
    // "forgot to roll back on the error path" bug class.
    // -----------------------------------------------------------------------
    class Transaction {
    public:
        explicit Transaction(Sqlite& db) : db_(&db) { db_->begin(); }
        ~Transaction() {
            if (db_) { try { db_->rollback(); } catch (...) { /* dtors don't throw */ } }
        }

        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;

        Transaction(Transaction&& o) noexcept : db_(o.db_) { o.db_ = nullptr; }
        Transaction& operator=(Transaction&& o) noexcept {
            if (this != &o) {
                if (db_) { try { db_->rollback(); } catch (...) {} }
                db_ = o.db_;
                o.db_ = nullptr;
            }
            return *this;
        }

        void commit()   { if (db_) { db_->commit();   db_ = nullptr; } }
        void rollback() { if (db_) { db_->rollback(); db_ = nullptr; } }
        bool active() const noexcept { return db_ != nullptr; }

    private:
        Sqlite* db_;
    };

    Statement prepare(const std::string& sql) {
        return Statement(*this, sql);
    }

    // Prepared-statement cache: prepare once per SQL string, reuse thereafter.
    // Returns a reference to the cached statement (stable across rehash — node-based
    // map). The caller should bind() (which resets+clears) before stepping.
    Statement& prepare_cached(const std::string& sql) {
        auto it = cache_.find(sql);
        if (it == cache_.end()) {
            it = cache_.try_emplace(sql, *this, sql).first;
        }
        return it->second;
    }

    // Drop the prepared-statement cache (e.g. before DDL that invalidates plans).
    void clear_cache() { cache_.clear(); }

    Transaction transaction() { return Transaction(*this); }

    template<typename... Args>
    void exec(const std::string& sql, Args&&... args) {
        Statement st(*this, sql);
        st.bind(std::forward<Args>(args)...);
        while (st.step()) {}
    }

#if defined(__cpp_lib_expected)
    // Non-throwing exec: SQL errors come back as std::unexpected(SqliteError).
    std::expected<void, SqliteError> try_exec(const std::string& sql) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            std::string msg = err ? err : "unknown error";
            sqlite3_free(err);
            return std::unexpected(SqliteError("exec failed: " + sql + " - " + msg));
        }
        return {};
    }
#endif

    // Escape hatch for advanced sqlite3 calls.
    sqlite3* handle() const noexcept { return db_; }

private:
    sqlite3* db_ = nullptr;
    std::unordered_map<std::string, Statement> cache_;
};

// ---------------------------------------------------------------------------
// Scalar "top-1" query helpers (jac313 addition, exported back to canonical)
// e.g. get_one_long(db, "SELECT n FROM t WHERE id=? LIMIT 1", id). Variadic binds.
// No row -> the type default (0 / "" / 0.0). Genuine SQL/prepare errors PROPAGATE.
// ---------------------------------------------------------------------------
template<class... B>
std::int64_t get_one_long(Sqlite& db, const char* sql, B&&... b) {
    std::int64_t v = 0;
    auto st = db.prepare(sql);
    if constexpr (sizeof...(B) > 0) st.bind(std::forward<B>(b)...);
    if (st.step()) st.get(v);
    return v;
}

template<class... B>
std::string get_one_string(Sqlite& db, const char* sql, B&&... b) {
    std::string v;
    auto st = db.prepare(sql);
    if constexpr (sizeof...(B) > 0) st.bind(std::forward<B>(b)...);
    if (st.step()) st.get(v);
    return v;
}

template<class... B>
double get_one_double(Sqlite& db, const char* sql, B&&... b) {
    double v = 0.0;
    auto st = db.prepare(sql);
    if constexpr (sizeof...(B) > 0) st.bind(std::forward<B>(b)...);
    if (st.step()) st.get(v);
    return v;
}

template<class... B>
std::int64_t get_one_long_or(Sqlite& db, const char* sql, std::int64_t deflt, B&&... b) {
    auto st = db.prepare(sql);
    if constexpr (sizeof...(B) > 0) st.bind(std::forward<B>(b)...);
    if (st.step()) { std::int64_t v = 0; st.get(v); return v; }
    return deflt;
}

template<class... B>
std::string get_one_string_or(Sqlite& db, const char* sql, std::string deflt, B&&... b) {
    auto st = db.prepare(sql);
    if constexpr (sizeof...(B) > 0) st.bind(std::forward<B>(b)...);
    if (st.step()) { std::string v; st.get(v); return v; }
    return deflt;
}

// Debug variants
template<class... B>
std::int64_t get_one_long(Sqlite& db, const char* sql, bool debug, B&&... b) {
    if (debug) std::cerr << "[SQL] " << sql << '\n';
    std::int64_t v = 0;
    auto st = db.prepare(sql);
    if constexpr (sizeof...(B) > 0) st.bind(std::forward<B>(b)...);
    if (st.step()) {
        st.get(v);
        if (debug) std::cerr << "[SQL] -> " << v << '\n';
    } else if (debug) {
        std::cerr << "[SQL] -> (no row)\n";
    }
    return v;
}

template<class... B>
std::string get_one_string(Sqlite& db, const char* sql, bool debug, B&&... b) {
    if (debug) std::cerr << "[SQL] " << sql << '\n';
    std::string v;
    auto st = db.prepare(sql);
    if constexpr (sizeof...(B) > 0) st.bind(std::forward<B>(b)...);
    if (st.step()) {
        st.get(v);
        if (debug) std::cerr << "[SQL] -> '" << v << "'\n";
    } else if (debug) {
        std::cerr << "[SQL] -> (no row)\n";
    }
    return v;
}

template<class... B>
double get_one_double(Sqlite& db, const char* sql, bool debug, B&&... b) {
    if (debug) std::cerr << "[SQL] " << sql << '\n';
    double v = 0.0;
    auto st = db.prepare(sql);
    if constexpr (sizeof...(B) > 0) st.bind(std::forward<B>(b)...);
    if (st.step()) {
        st.get(v);
        if (debug) std::cerr << "[SQL] -> " << v << '\n';
    } else if (debug) {
        std::cerr << "[SQL] -> (no row)\n";
    }
    return v;
}

template<class... B>
std::int64_t get_one_long_or(Sqlite& db, const char* sql, bool debug, std::int64_t deflt, B&&... b) {
    if (debug) std::cerr << "[SQL] " << sql << '\n';
    auto st = db.prepare(sql);
    if constexpr (sizeof...(B) > 0) st.bind(std::forward<B>(b)...);
    if (st.step()) {
        std::int64_t v = 0; st.get(v);
        if (debug) std::cerr << "[SQL] -> " << v << '\n';
        return v;
    }
    if (debug) std::cerr << "[SQL] -> (no row, using default " << deflt << ")\n";
    return deflt;
}

template<class... B>
std::string get_one_string_or(Sqlite& db, const char* sql, bool debug, std::string deflt, B&&... b) {
    if (debug) std::cerr << "[SQL] " << sql << '\n';
    auto st = db.prepare(sql);
    if constexpr (sizeof...(B) > 0) st.bind(std::forward<B>(b)...);
    if (st.step()) {
        std::string v; st.get(v);
        if (debug) std::cerr << "[SQL] -> '" << v << "'\n";
        return v;
    }
    if (debug) std::cerr << "[SQL] -> (no row, using default)\n";
    return deflt;
}

// Convenience debug shorthands
template<class... B> inline std::int64_t get_one_long_debug(Sqlite& db, const char* sql, B&&... b) {
    return get_one_long(db, sql, true, std::forward<B>(b)...);
}
template<class... B> inline std::string get_one_string_debug(Sqlite& db, const char* sql, B&&... b) {
    return get_one_string(db, sql, true, std::forward<B>(b)...);
}
template<class... B> inline double get_one_double_debug(Sqlite& db, const char* sql, B&&... b) {
    return get_one_double(db, sql, true, std::forward<B>(b)...);
}
template<class... B> inline std::int64_t get_one_long_or_debug(Sqlite& db, const char* sql, std::int64_t deflt, B&&... b) {
    return get_one_long_or(db, sql, true, deflt, std::forward<B>(b)...);
}
template<class... B> inline std::string get_one_string_or_debug(Sqlite& db, const char* sql, std::string deflt, B&&... b) {
    return get_one_string_or(db, sql, true, deflt, std::forward<B>(b)...);
}
