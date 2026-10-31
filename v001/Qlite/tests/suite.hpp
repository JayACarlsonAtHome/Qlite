// One test suite for every Qlite database: written once against the Database concept,
// run by sqlite_suite_test.cpp, postgres_suite_test.cpp and mariadb_suite_test.cpp. Each
// gives its placeholders and its CREATE TABLE; everything else is the same.

#pragma once

#include <cstdio>
#include <unistd.h>   // dup / dup2: the debug check reads stderr
#include <Qlite/v001/Database.hpp>

#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <vector>

namespace suite {

inline int failures = 0;

// A failed check says what and where, and the suite goes on.
inline void check(bool ok, std::string_view what, std::source_location at = std::source_location::current()) {
    if (ok) return;
    std::cerr << "FAIL: " << what << " at " << at.file_name() << ":" << at.line() << "\n";
    ++failures;
}

struct Dialect {
    std::string name;
    std::string (*placeholder)(int n);   // "?" or "$n"
    std::string create;                  // CREATE TABLE items (id, name, price, note, data)
};

template<Qlite::v001::Database D>
void run(D& db, const Dialect& d) {
    using Qlite::v001::get_top_one_bool;
    using Qlite::v001::get_top_one_bool_or;
    using Qlite::v001::get_top_one_double;
    using Qlite::v001::get_top_one_long;
    using Qlite::v001::get_top_one_long_or;
    using Qlite::v001::get_top_one_string;
    const auto p = d.placeholder;
    db.exec(d.create);

    const std::string odd = "O'Brien C:\\bits \xE2\x96\xAE";   // a quote, a backslash, U+25AE
    const std::vector<std::byte> blob{std::byte{0x00}, std::byte{0x01}, std::byte{0x7f}, std::byte{0xff}};
    const std::string insert = std::format("INSERT INTO items (id, name, price, note, data) VALUES ({}, {}, {}, {}, {})",
                                           p(1), p(2), p(3), p(4), p(5));
    auto ins = db.prepare(insert);
    ins.bind(std::int64_t{1}, odd, 3.25, std::optional<std::string>{}, std::span<const std::byte>(blob));
    check(!ins.step(), d.name + ": an INSERT has no rows");
    check(db.changes() == 1, d.name + ": changes() after an INSERT");
    ins.bind(std::int64_t{2}, std::string("plain"), 0.1, std::optional<std::string>("a note"),
             std::span<const std::byte>(blob).first(0));
    ins.step();

    auto sel = db.prepare("SELECT id, name, price, note FROM items ORDER BY id");
    check(sel.column_count() == 4 && sel.column_name(1) == "name", d.name + ": column metadata");
    std::int64_t id = 0;
    std::string name;
    double price = 0;
    std::optional<std::string> note;
    check(sel.step(), d.name + ": first row");
    sel.get(id, name, price, note);
    check(id == 1 && name == odd && price == 3.25 && !note,
          d.name + ": a quote, a backslash and U+25AE, a double, and NULL come back");
    check(sel.step(), d.name + ": second row");
    sel.get(id, name, price, note);
    check(id == 2 && name == "plain" && price == 0.1 && note == "a note", d.name + ": 0.1 exactly, a note");
    check(sel.template get_at<std::optional<std::string>>(3) == "a note" &&
              sel.template get_at<std::int64_t>(0) == 2 && sel.template get_at<std::string>(1) == "plain",
          d.name + ": get_at reads a column by its index");
    check(!sel.step(), d.name + ": two rows");

    auto b = db.prepare(std::format("SELECT data FROM items WHERE id = {}", p(1)));
    b.bind(std::int64_t{1});
    std::vector<std::byte> back;
    check(b.step(), d.name + ": the blob row");
    b.get(back);
    check(back == blob, d.name + ": the blob comes back byte for byte");
    b.bind(std::int64_t{2});
    check(b.step(), d.name + ": a statement runs again after a new bind");

    db.exec(std::format("UPDATE items SET name = {} WHERE id = {}", p(1), p(2)), std::string("plain"), std::int64_t{2});
    check(db.changes() == 1, d.name + ": an UPDATE to the same value counts its row");

    // changes() is the last DML's count: a transaction's COMMIT, a SELECT with rows and a
    // prepare leave it; a DELETE sets it.
    sel.reset();
    b.reset();
    db.exec("UPDATE items SET price = price");
    check(db.changes() == 2, d.name + ": an UPDATE of every row counts both");
    db.begin();
    db.commit();
    auto one = db.prepare(std::format("SELECT id FROM items WHERE id = {}", p(1)));
    one.bind(std::int64_t{1});
    check(one.step() && !one.step(), d.name + ": a SELECT of one row");
    db.prepare_cached(std::format("SELECT name FROM items WHERE price > {}", p(1)));
    check(db.changes() == 2, d.name + ": COMMIT, SELECT and prepare leave changes()");
    db.exec(std::format("DELETE FROM items WHERE id = {}", p(1)), std::int64_t{2});
    check(db.changes() == 1, d.name + ": a DELETE of one row counts it");
    ins.bind(std::int64_t{2}, std::string("plain"), 0.1, std::optional<std::string>("a note"),
             std::span<const std::byte>(blob).first(0));
    ins.step();

    check(get_top_one_long(db, "SELECT COUNT(*) FROM items") == 2, d.name + ": get_top_one_long");
    check(get_top_one_string(db, std::format("SELECT name FROM items WHERE id = {}", p(1)).c_str(), std::int64_t{1}) == odd,
          d.name + ": get_top_one_string with a bound value");
    check(get_top_one_double(db, std::format("SELECT price FROM items WHERE id = {}", p(1)).c_str(), std::int64_t{2}) == 0.1,
          d.name + ": get_top_one_double");
    check(get_top_one_long_or(db, std::format("SELECT id FROM items WHERE id = {}", p(1)).c_str(), std::int64_t{-1}, std::int64_t{99}) == -1,
          d.name + ": get_top_one_long_or's default when there is no row");

    check(get_top_one_bool(db, "SELECT 1 = 1") && !get_top_one_bool(db, "SELECT 1 = 0"), d.name + ": get_top_one_bool of a comparison");
    check(get_top_one_bool(db, std::format("SELECT COUNT(*) > 1 FROM items WHERE id <= {}", p(1)).c_str(), std::int64_t{2}),
          d.name + ": get_top_one_bool with a bound value");
    check(get_top_one_bool(db, "SELECT 'yes'") && !get_top_one_bool(db, "SELECT 'no'") && !get_top_one_bool(db, "SELECT NULL"),
          d.name + ": get_top_one_bool of text and NULL");
    check(!get_top_one_bool(db, std::format("SELECT 1 FROM items WHERE id = {}", p(1)).c_str(), std::int64_t{99}),
          d.name + ": get_top_one_bool with no row is false");
    check(get_top_one_bool_or(db, std::format("SELECT 0 FROM items WHERE id = {}", p(1)).c_str(), true, std::int64_t{99}),
          d.name + ": get_top_one_bool_or's default when there is no row");
    check(get_top_one_bool(db, "SELECT 1 = 1", false), d.name + ": get_top_one_bool with debug off");
    check(get_top_one_long(db, "SELECT id FROM items ORDER BY id DESC") == 2 &&
          get_top_one_long(db, "SELECT id FROM items ORDER BY id") == 1, d.name + ": the top row of many, by ORDER BY");
    using Qlite::v001::get_top_one;
    using Qlite::v001::get_top_one_or;
    check(get_top_one<std::string>(db, std::format("SELECT name FROM items WHERE id = {}", p(1)).c_str(), std::int64_t{2}) == "plain",
          d.name + ": get_top_one<std::string>");
    check(!get_top_one<std::optional<std::int64_t>>(db, std::format("SELECT id FROM items WHERE id = {}", p(1)).c_str(), std::int64_t{99}),
          d.name + ": get_top_one<optional> with no row is nullopt (not an id)");
    check(get_top_one<std::optional<std::int64_t>>(db, "SELECT id FROM items ORDER BY id") == 1, d.name + ": get_top_one<optional> of a row");
    check(!get_top_one<std::optional<std::string>>(db, "SELECT note FROM items ORDER BY id"), d.name + ": get_top_one<optional> of NULL");
    check(get_top_one_string_or(db, std::format("SELECT name FROM items WHERE id = {}", p(1)).c_str(), "none", std::int64_t{99}) == "none",
          d.name + ": a text default is the default, not debug");
    check(get_top_one_or<double>(db, std::format("SELECT price FROM items WHERE id = {}", p(1)).c_str(), 7.5, std::int64_t{99}) == 7.5,
          d.name + ": get_top_one_or<double>'s default");

    // ---- get_top_one: every type x a value, a zero / empty value, NULL and no row ----
    {
        using namespace Qlite::v001;
        db.exec("DROP TABLE IF EXISTS top_one");
        db.exec("CREATE TABLE top_one (id INTEGER PRIMARY KEY, n BIGINT, x DOUBLE PRECISION, b BOOLEAN, s VARCHAR(40))");
        db.exec(std::format("INSERT INTO top_one (id, n, x, b, s) VALUES ({}, {}, {}, {}, {})", p(1), p(2), p(3), p(4), p(5)),
                std::int64_t{1}, std::int64_t{42}, 2.5, 1, std::string("forty-two"));                 // a value
        db.exec(std::format("INSERT INTO top_one (id, n, x, b, s) VALUES ({}, {}, {}, {}, {})", p(1), p(2), p(3), p(4), p(5)),
                std::int64_t{2}, std::int64_t{0}, 0.0, 0, std::string());                             // zero / empty
        db.exec(std::format("INSERT INTO top_one (id) VALUES ({})", p(1)), std::int64_t{3});           // all NULL
        const auto q = [&](const char* col) { return std::format("SELECT {} FROM top_one WHERE id = {}", col, p(1)); };
        const std::string n = q("n"), x = q("x"), bq = q("b"), s = q("s");
        const std::int64_t VAL = 1, ZERO = 2, NUL = 3, NONE = 99;
        using I = std::optional<std::int64_t>;
        using X = std::optional<double>;
        using Bo = std::optional<bool>;
        using S = std::optional<std::string>;

        // plain T: the value; NULL and no row both read as the type's empty value
        check(get_top_one_long(db, n.c_str(), VAL) == 42 && get_top_one_long(db, n.c_str(), ZERO) == 0 &&
              get_top_one_long(db, n.c_str(), NUL) == 0 && get_top_one_long(db, n.c_str(), NONE) == 0, d.name + ": top-one long: 42, 0, NULL 0, no row 0");
        check(get_top_one_id(db, std::format("SELECT id FROM top_one WHERE s = {}", p(1)).c_str(), std::string("forty-two")) == 1 &&
              get_top_one_id(db, std::format("SELECT id FROM top_one WHERE s = {}", p(1)).c_str(), std::string("absent")) == 0 &&
              get_top_one_id(db, n.c_str(), NUL) == 0 && get_top_one_id(db, "SELECT id FROM top_one ORDER BY id DESC") == 3,
              d.name + ": top-one id: found 1, none 0, NULL 0, the top of many 3");
        check(get_top_one_id_or(db, std::format("SELECT id FROM top_one WHERE s = {}", p(1)).c_str(), std::int64_t{-1}, std::string("absent")) == -1 &&
              get_top_one_id_or(db, n.c_str(), std::int64_t{-1}, NUL) == -1 && get_top_one_id_or(db, n.c_str(), std::int64_t{-1}, ZERO) == 0 &&
              get_top_one_id_or(db, n.c_str(), false, std::int64_t{-1}, VAL) == 42,
              d.name + ": top-one id_or: none -1, NULL -1, a real 0 stays 0, with debug off");
        check(get_top_one<int>(db, n.c_str(), VAL) == 42 && get_top_one<int>(db, n.c_str(), NUL) == 0, d.name + ": top-one int");
        check(get_top_one_double(db, x.c_str(), VAL) == 2.5 && get_top_one_double(db, x.c_str(), ZERO) == 0.0 &&
              get_top_one_double(db, x.c_str(), NUL) == 0.0 && get_top_one_double(db, x.c_str(), NONE) == 0.0, d.name + ": top-one double: 2.5, 0, NULL 0, no row 0");
        check(get_top_one_bool(db, bq.c_str(), VAL) && !get_top_one_bool(db, bq.c_str(), ZERO) &&
              !get_top_one_bool(db, bq.c_str(), NUL) && !get_top_one_bool(db, bq.c_str(), NONE), d.name + ": top-one bool: true, false, NULL false, no row false");
        check(get_top_one_string(db, s.c_str(), VAL) == "forty-two" && get_top_one_string(db, s.c_str(), ZERO).empty() &&
              get_top_one_string(db, s.c_str(), NUL).empty() && get_top_one_string(db, s.c_str(), NONE).empty(), d.name + ": top-one string: value, \"\", NULL \"\", no row \"\"");

        // optional<T>: a value (also 0, false, "") has a value; NULL and no row are nullopt
        check(get_top_one<I>(db, n.c_str(), VAL) == 42 && get_top_one<I>(db, n.c_str(), ZERO) == 0 &&
              !get_top_one<I>(db, n.c_str(), NUL) && !get_top_one<I>(db, n.c_str(), NONE), d.name + ": top-one optional<int64>: 42, 0, NULL none, no row none");
        check(get_top_one<X>(db, x.c_str(), VAL) == 2.5 && get_top_one<X>(db, x.c_str(), ZERO) == 0.0 &&
              !get_top_one<X>(db, x.c_str(), NUL) && !get_top_one<X>(db, x.c_str(), NONE), d.name + ": top-one optional<double>");
        check(get_top_one<Bo>(db, bq.c_str(), VAL) == true && get_top_one<Bo>(db, bq.c_str(), ZERO) == false &&
              !get_top_one<Bo>(db, bq.c_str(), NUL) && !get_top_one<Bo>(db, bq.c_str(), NONE), d.name + ": top-one optional<bool>: NULL is none, not false");
        check(get_top_one<S>(db, s.c_str(), VAL) == "forty-two" && get_top_one<S>(db, s.c_str(), ZERO) == std::string() &&
              !get_top_one<S>(db, s.c_str(), NUL) && !get_top_one<S>(db, s.c_str(), NONE), d.name + ": top-one optional<string>: \"\" is a value, NULL none");

        // _or: the default is for no row only; a NULL row reads as above
        check(get_top_one_long_or(db, n.c_str(), std::int64_t{-7}, VAL) == 42 && get_top_one_long_or(db, n.c_str(), std::int64_t{-7}, NUL) == 0 &&
              get_top_one_long_or(db, n.c_str(), std::int64_t{-7}, NONE) == -7, d.name + ": top-one long_or");
        check(get_top_one_or<double>(db, x.c_str(), 7.5, NUL) == 0.0 && get_top_one_or<double>(db, x.c_str(), 7.5, NONE) == 7.5, d.name + ": top-one or<double>");
        check(!get_top_one_bool_or(db, bq.c_str(), true, NUL) && get_top_one_bool_or(db, bq.c_str(), true, NONE), d.name + ": top-one bool_or");
        check(get_top_one_string_or(db, s.c_str(), "none", ZERO).empty() && get_top_one_string_or(db, s.c_str(), "none", NONE) == "none",
              d.name + ": top-one string_or (a text default, not debug)");
        check(get_top_one_or<I>(db, n.c_str(), I{5}, NUL) == std::nullopt && get_top_one_or<I>(db, n.c_str(), I{5}, NONE) == 5,
              d.name + ": top-one or<optional>: NULL none, no row the default");

        // debug: the SQL and the result go to stderr, and the result is unchanged
        const auto stderr_of = [](auto fn) {
            std::fflush(stderr);
            const int saved = ::dup(2);
            std::FILE* tmp = std::tmpfile();
            ::dup2(::fileno(tmp), 2);
            fn();
            std::fflush(stderr);
            ::dup2(saved, 2);
            ::close(saved);
            std::rewind(tmp);
            std::string out;
            for (int c; (c = std::fgetc(tmp)) != EOF;) out += static_cast<char>(c);
            std::fclose(tmp);
            return out;
        };
        std::int64_t got = 0;
        std::string log = stderr_of([&] { got = get_top_one_long(db, n.c_str(), true, VAL); });
        check(got == 42 && log.find("[SQL] SELECT n FROM top_one") != std::string::npos && log.find("[SQL] -> 42") != std::string::npos,
              d.name + ": debug prints the SQL and 42");
        log = stderr_of([&] { (void)get_top_one<I>(db, n.c_str(), true, NUL); });
        check(log.find("[SQL] -> NULL") != std::string::npos, d.name + ": debug prints NULL");
        log = stderr_of([&] { (void)get_top_one_string(db, s.c_str(), true, NONE); });
        check(log.find("(no row)") != std::string::npos, d.name + ": debug prints no row");
        log = stderr_of([&] { (void)get_top_one_bool_debug(db, bq.c_str(), VAL); });
        check(log.find("[SQL] -> true") != std::string::npos, d.name + ": _debug prints true");
        log = stderr_of([&] { (void)get_top_one_string(db, s.c_str(), false, VAL); });
        check(log.empty(), d.name + ": debug off prints nothing");
    }

    const auto count = [&] { return get_top_one_long(db, "SELECT COUNT(*) FROM items"); };
    {
        auto t = db.transaction();
        db.exec(insert, std::int64_t{3}, std::string("rolled back"), 1.0, std::optional<std::string>{},
                std::span<const std::byte>(blob));
    }
    check(count() == 2, d.name + ": a transaction left uncommitted rolls back");
    {
        auto t = db.transaction();
        db.exec(insert, std::int64_t{3}, std::string("kept"), 1.0, std::optional<std::string>{},
                std::span<const std::byte>(blob));
        t.commit();
    }
    check(count() == 3, d.name + ": a committed transaction keeps its row");

    auto& cached = db.prepare_cached("SELECT COUNT(*) FROM items");
    check(&cached == &db.prepare_cached("SELECT COUNT(*) FROM items"), d.name + ": prepare_cached reuses");

    bool threw = false;
    try { db.exec("SELECT nope FROM no_such_table"); }
    catch (const Qlite::v001::Error&) { threw = true; }
    check(threw, d.name + ": a bad statement throws Qlite::v001::Error");

    // Two statements in one prepare(): the second would never run, so it is refused.
    threw = false;
    try { (void)db.prepare("SELECT 1; SELECT 2"); }
    catch (const Qlite::v001::Error&) { threw = true; }
    check(threw, d.name + ": prepare() refuses two statements");

    // A statement moved after it stepped still reads its row and steps on.
    {
        auto first = db.prepare("SELECT id, name FROM items ORDER BY id");
        check(first.step(), d.name + ": a row to move with");
        auto second = std::move(first);
        std::int64_t mid = 0;
        std::string mname;
        second.get(mid, mname);
        check(mid == 1 && mname == odd, d.name + ": a moved statement reads its row");
        auto third = db.prepare("SELECT 1");
        third = std::move(second);
        check(third.step(), d.name + ": a move-assigned statement steps on");
        third.get(mid, mname);
        check(mid == 2 && mname == "plain", d.name + ": and reads the next row");
    }

    // A moved connection keeps its cached statements, and they point at the new object.
    {
        D moved(std::move(db));
        auto& again = moved.prepare_cached("SELECT COUNT(*) FROM items");
        check(&again == &cached, d.name + ": the cache moved with the connection");
        std::int64_t n = 0;
        check(again.step(), d.name + ": a cached statement steps after the connection moved");
        again.get(n);
        check(n == 3, d.name + ": and reads after the connection moved");
        again.reset();
        db = std::move(moved);
        n = 0;
        check(cached.step(), d.name + ": a cached statement steps after the connection moved back");
        cached.get(n);
        check(n == 3, d.name + ": and reads after it moved back");
        cached.reset();
    }
}

}  // namespace suite
