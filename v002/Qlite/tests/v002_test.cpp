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
#include <source_location>
#include <string_view>
#include <cstdlib>

static_assert(Qlite::v002::version() == "v002.016", "v002 version");

using Qlite::v002::Sqlite;
using Qlite::v002::SqliteError;

namespace {

int failures = 0;

// A failed check says what and where, and counts toward failures.
void check(bool ok, std::string_view what, std::source_location at = std::source_location::current()) {
    if (ok) return;
    std::cerr << "FAIL: " << what << " at " << at.file_name() << ":" << at.line() << "\n";
    ++failures;
}

// ---------------------------------------------------------------------------
// Part 1 — frozen surface
// ---------------------------------------------------------------------------

void test_exec_bind_get() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT, score REAL)");
    db.exec("INSERT INTO t (name, score) VALUES (?, ?)", std::string("qlite"), 3.5);

    auto stmt = db.prepare("SELECT id, name, score FROM t");
    check(stmt.step(), "stmt.step()");
    std::int64_t id = 0;
    std::string name;
    double score = 0.0;
    stmt.get(id, name, score);
    check(id == 1, "id == 1");
    check(name == "qlite", "name == \"qlite\"");
    check(score == 3.5, "score == 3.5");
    check(!stmt.step(), "!stmt.step()");
}

void test_prepared_reuse_and_rows() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE t (n INTEGER)");
    auto ins = db.prepare("INSERT INTO t (n) VALUES (?)");
    for (std::int64_t i = 1; i <= 5; ++i) {
        ins.bind(i);
        check(!ins.step(), "!ins.step()");
    }

    auto sel = db.prepare("SELECT n FROM t ORDER BY n");
    std::int64_t sum = 0, rows = 0;
    while (sel.step()) {
        auto [n] = sel.get_row<std::int64_t>();
        sum += n;
        ++rows;
    }
    check(rows == 5, "rows == 5");
    check(sum == 15, "sum == 15");
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
    check(sel.step(), "sel.step()");
    std::int64_t a = 0; double b = 0; std::string c;
    sel.get(a, b, c);
    check(a == 7, "a == 7");
    check(b == 2.5, "b == 2.5");
    check(c == "metric", "c == \"metric\"");
}

void test_optional_null() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE t (id INTEGER, label TEXT)");
    db.exec("INSERT INTO t VALUES (?, ?)", 1, std::optional<std::string>{});
    db.exec("INSERT INTO t VALUES (?, ?)", 2, std::optional<std::string>{"set"});

    auto stmt = db.prepare("SELECT label FROM t ORDER BY id");
    check(stmt.step(), "stmt.step()");
    std::optional<std::string> a;
    stmt.get(a);
    check(!a.has_value(), "!a.has_value()");
    check(stmt.step(), "stmt.step()");
    std::optional<std::string> b;
    stmt.get(b);
    check(b.has_value(), "b.has_value()");
    check(*b == "set", "*b == \"set\"");
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
    check(stmt.step(), "stmt.step()");
    std::int64_t count = 0;
    stmt.get(count);
    check(count == 1, "count == 1");
}

void test_error_on_bad_sql() {
    Sqlite db(":memory:");
    bool threw = false;
    try {
        db.exec("THIS IS NOT VALID SQL");
    } catch (const SqliteError& e) {
        threw = true;
        check(e.code() != 0 || std::string(e.what()).find("exec failed") != std::string::npos, "e.code() != 0 || std::string(e.what()).find(\"exec failed\") != std::string::npos");
    }
    check(threw, "threw");
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
        check(tx.active(), "tx.active()");
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
    check(stmt.step(), "stmt.step()");
    std::int64_t count = 0;
    stmt.get(count);
    check(count == 1, "count == 1");   // only the committed row (10) survives
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
    check(&db.prepare_cached(sql) == &db.prepare_cached(sql), "&db.prepare_cached(sql) == &db.prepare_cached(sql)");

    auto sel = db.prepare("SELECT SUM(n) FROM t");
    check(sel.step(), "sel.step()");
    std::int64_t sum = 0;
    sel.get(sum);
    check(sum == 6, "sum == 6");
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
    check(rows.size() == 2, "rows.size() == 2");
    check(std::get<1>(rows[0]) == "view-bound", "std::get<1>(rows[0]) == \"view-bound\"");
    check(std::get<1>(rows[1]) == "named", "std::get<1>(rows[1]) == \"named\"");
}

void test_column_by_name() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE t (id INTEGER, name TEXT, score REAL)");
    db.exec("INSERT INTO t VALUES (?, ?, ?)", 1, std::string("byname"), 9.5);

    auto stmt = db.prepare("SELECT id, name, score FROM t");
    check(stmt.step(), "stmt.step()");
    check(stmt.column_count() == 3, "stmt.column_count() == 3");
    check(stmt.column_name(1) == "name", "stmt.column_name(1) == \"name\"");
    check(stmt.column_index("score") == 2, "stmt.column_index(\"score\") == 2");
    check(stmt.get_by_name<std::int64_t>("id") == 1, "stmt.get_by_name<std::int64_t>(\"id\") == 1");
    check(stmt.get_by_name<std::string>("name") == "byname", "stmt.get_by_name<std::string>(\"name\") == \"byname\"");
    check(stmt.get_by_name<double>("score") == 9.5, "stmt.get_by_name<double>(\"score\") == 9.5");
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
    check(sel.step(), "sel.step()");
    auto got = sel.get_row<std::vector<std::byte>>();
    check(std::get<0>(got) == payload, "std::get<0>(got) == payload");
}

void test_expected_path() {
#if defined(__cpp_lib_expected)
    Sqlite db(":memory:");
    auto ok = db.try_exec("CREATE TABLE t (n INTEGER)");
    check(ok.has_value(), "ok.has_value()");
    auto bad = db.try_exec("NOPE NOT SQL");
    check(!bad.has_value(), "!bad.has_value()");
    check(std::string(bad.error().what()).find("exec failed") != std::string::npos, "std::string(bad.error().what()).find(\"exec failed\") != std::string::npos");

    db.exec("INSERT INTO t VALUES (1)");
    auto sel = db.prepare("SELECT n FROM t");
    auto r = sel.try_step();
    check(r.has_value(), "r.has_value()");
    check(*r == true, "*r == true");
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
    check(stmt.step(), "stmt.step()");
    int first = 0;
    stmt.get(first);                        // get(int&) -> column<int>
    check(first == -3, "first == -3");
    check(stmt.get_by_name<int>("n") == -3, "stmt.get_by_name<int>(\"n\") == -3"); // get_by_name<int> -> column<int>
    check(stmt.step(), "stmt.step()");
    int second = 0;
    stmt.get(second);
    check(second == 7, "second == 7");
    check(!stmt.step(), "!stmt.step()");

    auto all = db.prepare("SELECT n FROM t ORDER BY n").get_all<int>();  // get_all<int>
    check(all.size() == 2, "all.size() == 2");
    check(std::get<0>(all[0]) == -3, "std::get<0>(all[0]) == -3");
    check(std::get<0>(all[1]) == 7, "std::get<0>(all[1]) == 7");
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
