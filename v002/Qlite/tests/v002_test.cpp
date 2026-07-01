// Unit tests for the Qlite::v002 SQLite wrapper (C++23).
//
// Part 1 — the FROZEN surface (what downstream consumers, e.g. jac313's Store SQL
// sink, depend on): exec/bind/get, prepared-statement reuse, indexed runtime binds,
// optional/NULL handling, transactions (commit + rollback), error reporting.
//
// Part 2 — the ADDITIVE features: RAII transaction guard, prepared-statement cache,
// string_view/blob binds, column-by-name, named parameters, get_all, and (when the
// toolchain provides <expected>) the non-throwing try_* path.

#include <Qlite/v002.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <vector>

static_assert(Qlite::v002::version() == "v002.004", "v002 version");

using Qlite::v002::Sqlite;
using Qlite::v002::SqliteError;

namespace {

int failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL: " #cond " at " << __FILE__ << ":" << __LINE__  \
                      << "\n";                                                  \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

// ---------------------------------------------------------------------------
// Part 1 — frozen surface
// ---------------------------------------------------------------------------

void test_exec_bind_get() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT, score REAL)");
    db.exec("INSERT INTO t (name, score) VALUES (?, ?)", std::string("qlite"), 3.5);

    auto stmt = db.prepare("SELECT id, name, score FROM t");
    CHECK(stmt.step());
    std::int64_t id = 0;
    std::string name;
    double score = 0.0;
    stmt.get(id, name, score);
    CHECK(id == 1);
    CHECK(name == "qlite");
    CHECK(score == 3.5);
    CHECK(!stmt.step());
}

void test_prepared_reuse_and_rows() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE t (n INTEGER)");
    auto ins = db.prepare("INSERT INTO t (n) VALUES (?)");
    for (std::int64_t i = 1; i <= 5; ++i) {
        ins.bind(i);
        CHECK(!ins.step());
    }

    auto sel = db.prepare("SELECT n FROM t ORDER BY n");
    std::int64_t sum = 0, rows = 0;
    while (sel.step()) {
        auto [n] = sel.get_row<std::int64_t>();
        sum += n;
        ++rows;
    }
    CHECK(rows == 5);
    CHECK(sum == 15);
}

void test_indexed_runtime_binds() {
    // Mirrors how Store's SqlEventSink fills dynamic metric columns.
    Sqlite db(":memory:");
    db.exec("CREATE TABLE m (a INTEGER, b REAL, c TEXT)");
    auto ins = db.prepare("INSERT INTO m (a, b, c) VALUES (?, ?, ?)");
    ins.reset();
    ins.clear_bindings();
    int idx = 1;
    ins.bind_int64(idx++, 7);
    ins.bind_double(idx++, 2.5);
    ins.bind_text(idx++, std::string("metric"));
    while (ins.step()) {}

    auto sel = db.prepare("SELECT a, b, c FROM m");
    CHECK(sel.step());
    std::int64_t a = 0; double b = 0; std::string c;
    sel.get(a, b, c);
    CHECK(a == 7);
    CHECK(b == 2.5);
    CHECK(c == "metric");
}

void test_optional_null() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE t (id INTEGER, label TEXT)");
    db.exec("INSERT INTO t VALUES (?, ?)", 1, std::optional<std::string>{});
    db.exec("INSERT INTO t VALUES (?, ?)", 2, std::optional<std::string>{"set"});

    auto stmt = db.prepare("SELECT label FROM t ORDER BY id");
    CHECK(stmt.step());
    std::optional<std::string> a;
    stmt.get(a);
    CHECK(!a.has_value());
    CHECK(stmt.step());
    std::optional<std::string> b;
    stmt.get(b);
    CHECK(b.has_value());
    CHECK(*b == "set");
}

void test_transaction_commit_and_rollback() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE t (n INTEGER)");

    db.begin();
    db.exec("INSERT INTO t (n) VALUES (1)");
    db.commit();

    db.begin();
    db.exec("INSERT INTO t (n) VALUES (2)");
    db.rollback();

    auto stmt = db.prepare("SELECT COUNT(*) FROM t");
    CHECK(stmt.step());
    std::int64_t count = 0;
    stmt.get(count);
    CHECK(count == 1);
}

void test_error_on_bad_sql() {
    Sqlite db(":memory:");
    bool threw = false;
    try {
        db.exec("THIS IS NOT VALID SQL");
    } catch (const SqliteError& e) {
        threw = true;
        CHECK(e.code() != 0 || std::string(e.what()).find("exec failed") != std::string::npos);
    }
    CHECK(threw);
}

// ---------------------------------------------------------------------------
// Part 2 — additive features
// ---------------------------------------------------------------------------

void test_transaction_guard_autorollback() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE t (n INTEGER)");

    // Commit path.
    {
        auto tx = db.transaction();
        db.exec("INSERT INTO t (n) VALUES (10)");
        tx.commit();
    }
    // Auto-rollback path: leave scope without commit().
    {
        auto tx = db.transaction();
        db.exec("INSERT INTO t (n) VALUES (20)");
        CHECK(tx.active());
        // no commit -> dtor rolls back
    }
    // Exception path: guard rolls back as the stack unwinds.
    try {
        auto tx = db.transaction();
        db.exec("INSERT INTO t (n) VALUES (30)");
        throw std::runtime_error("boom");
    } catch (const std::runtime_error&) {
    }

    auto stmt = db.prepare("SELECT COUNT(*) FROM t");
    CHECK(stmt.step());
    std::int64_t count = 0;
    stmt.get(count);
    CHECK(count == 1);   // only the committed row (10) survives
}

void test_prepared_statement_cache() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE t (n INTEGER)");

    const std::string sql = "INSERT INTO t (n) VALUES (?)";
    for (std::int64_t i = 1; i <= 3; ++i) {
        Sqlite::Statement& st = db.prepare_cached(sql);   // same object each call
        st.bind(i);
        while (st.step()) {}
    }
    // Same SQL string returns the same cached statement instance.
    CHECK(&db.prepare_cached(sql) == &db.prepare_cached(sql));

    auto sel = db.prepare("SELECT SUM(n) FROM t");
    CHECK(sel.step());
    std::int64_t sum = 0;
    sel.get(sum);
    CHECK(sum == 6);
}

void test_string_view_and_named_params() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE t (id INTEGER, name TEXT)");

    std::string_view name = "view-bound";
    db.exec("INSERT INTO t VALUES (?, ?)", 1, name);

    auto ins = db.prepare("INSERT INTO t (id, name) VALUES (:id, :name)");
    ins.bind_param(":id", 2);
    ins.bind_param(":name", std::string("named"));
    while (ins.step()) {}

    auto sel = db.prepare("SELECT id, name FROM t ORDER BY id");
    auto rows = sel.get_all<std::int64_t, std::string>();
    CHECK(rows.size() == 2);
    CHECK(std::get<1>(rows[0]) == "view-bound");
    CHECK(std::get<1>(rows[1]) == "named");
}

void test_column_by_name() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE t (id INTEGER, name TEXT, score REAL)");
    db.exec("INSERT INTO t VALUES (?, ?, ?)", 1, std::string("byname"), 9.5);

    auto stmt = db.prepare("SELECT id, name, score FROM t");
    CHECK(stmt.step());
    CHECK(stmt.column_count() == 3);
    CHECK(stmt.column_name(1) == "name");
    CHECK(stmt.column_index("score") == 2);
    CHECK(stmt.get_by_name<std::int64_t>("id") == 1);
    CHECK(stmt.get_by_name<std::string>("name") == "byname");
    CHECK(stmt.get_by_name<double>("score") == 9.5);
}

void test_blob_roundtrip() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE b (id INTEGER, data BLOB)");

    std::vector<std::byte> payload{std::byte{0x00}, std::byte{0xDE}, std::byte{0xAD},
                                   std::byte{0xBE}, std::byte{0xEF}};
    auto ins = db.prepare("INSERT INTO b VALUES (?, ?)");
    ins.bind(1, std::span<const std::byte>(payload));
    while (ins.step()) {}

    auto sel = db.prepare("SELECT data FROM b");
    CHECK(sel.step());
    auto got = sel.get_row<std::vector<std::byte>>();
    CHECK(std::get<0>(got) == payload);
}

void test_expected_path() {
#if defined(__cpp_lib_expected)
    Sqlite db(":memory:");
    auto ok = db.try_exec("CREATE TABLE t (n INTEGER)");
    CHECK(ok.has_value());
    auto bad = db.try_exec("NOPE NOT SQL");
    CHECK(!bad.has_value());
    CHECK(std::string(bad.error().what()).find("exec failed") != std::string::npos);

    db.exec("INSERT INTO t VALUES (1)");
    auto sel = db.prepare("SELECT n FROM t");
    auto r = sel.try_step();
    CHECK(r.has_value());
    CHECK(*r == true);
#else
    std::cout << "  (std::expected unavailable — try_* path skipped)\n";
#endif
}

// column<int> extractor: read an INTEGER column straight into a plain `int`
// (distinct from the std::int64_t path). Covers every route through column<T> —
// get(int&), get_by_name<int>, and get_all<int>.
void test_column_int() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE t (n INTEGER)");
    db.exec("INSERT INTO t VALUES (?)", std::int64_t{-3});
    db.exec("INSERT INTO t VALUES (?)", std::int64_t{7});

    auto stmt = db.prepare("SELECT n FROM t ORDER BY n");
    CHECK(stmt.step());
    int first = 0;
    stmt.get(first);                        // get(int&) -> column<int>
    CHECK(first == -3);
    CHECK(stmt.get_by_name<int>("n") == -3); // get_by_name<int> -> column<int>
    CHECK(stmt.step());
    int second = 0;
    stmt.get(second);
    CHECK(second == 7);
    CHECK(!stmt.step());

    auto all = db.prepare("SELECT n FROM t ORDER BY n").get_all<int>();  // get_all<int>
    CHECK(all.size() == 2);
    CHECK(std::get<0>(all[0]) == -3);
    CHECK(std::get<0>(all[1]) == 7);
}

} // namespace

int main() {
    test_exec_bind_get();
    test_prepared_reuse_and_rows();
    test_indexed_runtime_binds();
    test_optional_null();
    test_transaction_commit_and_rollback();
    test_error_on_bad_sql();

    test_transaction_guard_autorollback();
    test_prepared_statement_cache();
    test_string_view_and_named_params();
    test_column_by_name();
    test_column_int();
    test_blob_roundtrip();
    test_expected_path();

    if (failures == 0) {
        std::cout << "Qlite::v002 — all frozen-surface + additive-feature tests passed\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
