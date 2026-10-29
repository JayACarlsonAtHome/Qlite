// The Qlite suite (suite.hpp) on SQLite, in memory.
#include "suite.hpp"
#include <Qlite/v001/Sqlite.hpp>

static_assert(Qlite::v001::Database<Qlite::v001::Sqlite>);

int main() {
    Qlite::v001::Sqlite db(":memory:");
    suite::run(db, {"SQLite", [](int) { return std::string("?"); },
                    "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT NOT NULL, price REAL, note TEXT, data BLOB)"});
    std::cout << (suite::failures ? "SQLite suite FAILED\n" : "SQLite suite passed\n");
    return suite::failures ? 1 : 0;
}
