// The Qlite suite (suite.hpp) on MariaDB, as the current login over the local socket, in a
// throwaway database dropped after (the login needs GRANT ALL ON `qlite\_test\_%`.*).
// Exits 77 (skipped) when no server answers or the database cannot be made.
#include "suite.hpp"
#include <Qlite/v002/MariaDB.hpp>
#include <unistd.h>

static_assert(Qlite::v002::Database<Qlite::v002::MariaDB>);

int main() {
    std::optional<Qlite::v002::MariaDB> db;
    try { db.emplace(Qlite::v002::MariaDB::Options{}); }
    catch (const Qlite::v002::Error& e) {
        std::cout << "MariaDB: no server answers; skipped (" << e.what() << ")\n";
        return 77;
    }
    const std::string name = std::format("qlite_test_{}", ::getpid());
    try { db->exec(std::format("CREATE DATABASE {0}; USE {0}", name)); }
    catch (const Qlite::v002::Error& e) {
        std::cout << "MariaDB: cannot make the throwaway database (GRANT ALL ON `qlite\\_test\\_%`.* "
                     "to the login); skipped (" << e.what() << ")\n";
        return 77;
    }
    try {
        suite::run(*db, {"MariaDB", [](int) { return std::string("?"); },
                         "CREATE TABLE items (id BIGINT PRIMARY KEY, name TEXT NOT NULL, "
                         "price DOUBLE, note TEXT, data BLOB)"});
    } catch (const std::exception& e) {
        suite::check(false, std::string("MariaDB: ") + e.what());
    }
    db->clear_cache();
    try { db->exec("ROLLBACK"); } catch (...) {}
    db->exec(std::format("DROP DATABASE {}", name));
    std::cout << (suite::failures ? "MariaDB suite FAILED\n" : "MariaDB suite passed\n");
    return suite::failures ? 1 : 0;
}
