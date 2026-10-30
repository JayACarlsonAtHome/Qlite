// The Qlite suite (suite.hpp) on SQLite, in memory.
#include "suite.hpp"
#include <Qlite/v002/Sqlite.hpp>

static_assert(Qlite::v002::Database<Qlite::v002::Sqlite>);

int main() {
    Qlite::v002::Sqlite db(":memory:");
    suite::run(db, {"SQLite", [](int) { return std::string("?"); },
                    "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT NOT NULL, price REAL, note TEXT, data BLOB)"});
    // A step() error carries SQLite's message, not just its code.
    try {
        db.exec("INSERT INTO items (id, name) VALUES (?, ?)", std::int64_t{1}, std::string("again"));
        suite::check(false, "SQLite: a duplicate id throws");
    } catch (const Qlite::v002::SqliteError& e) {
        suite::check(std::string_view(e.what()).find("UNIQUE constraint failed") != std::string_view::npos,
                     "SQLite: a step error's what() has the message");
    }
    std::cout << (suite::failures ? "SQLite suite FAILED\n" : "SQLite suite passed\n");
    return suite::failures ? 1 : 0;
}
