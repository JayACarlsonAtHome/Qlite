// One test suite for every Qlite database: written once against the Database concept,
// run by sqlite_suite_test.cpp, postgres_suite_test.cpp and mariadb_suite_test.cpp. Each
// gives its placeholders and its CREATE TABLE; everything else is the same.

#pragma once

#include <Qlite/v002/Database.hpp>

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

template<Qlite::v002::Database D>
void run(D& db, const Dialect& d) {
    using Qlite::v002::get_top_one_bool;
    using Qlite::v002::get_top_one_bool_or;
    using Qlite::v002::get_top_one_double;
    using Qlite::v002::get_top_one_long;
    using Qlite::v002::get_top_one_long_or;
    using Qlite::v002::get_top_one_string;
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
    using Qlite::v002::get_top_one;
    using Qlite::v002::get_top_one_or;
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
    catch (const Qlite::v002::Error&) { threw = true; }
    check(threw, d.name + ": a bad statement throws Qlite::v002::Error");
}

}  // namespace suite
