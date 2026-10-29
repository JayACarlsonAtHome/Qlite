// The Qlite suite (suite.hpp) on PostgreSQL, through the standard environment (PG*), in a
// throwaway schema dropped after. Exits 77 (skipped) when no server answers.
#include "suite.hpp"
#include <Qlite/v001/Postgres.hpp>
#include <unistd.h>

static_assert(Qlite::v001::Database<Qlite::v001::Postgres>);

int main() {
    std::optional<Qlite::v001::Postgres> db;
    try { db.emplace(); }
    catch (const Qlite::v001::Error& e) {
        std::cout << "PostgreSQL: no server answers; skipped (" << e.what() << ")\n";
        return 77;
    }
    const std::string schema = std::format("qlite_test_{}", ::getpid());
    db->exec(std::format("SET client_min_messages TO warning; CREATE SCHEMA {0}; SET search_path TO {0}", schema));
    try {
        suite::run(*db, {"PostgreSQL", [](int n) { return std::format("${}", n); },
                         "CREATE TABLE items (id BIGINT PRIMARY KEY, name TEXT NOT NULL, "
                         "price DOUBLE PRECISION, note TEXT, data BYTEA)"});
    } catch (const std::exception& e) {
        suite::check(false, std::string("PostgreSQL: ") + e.what());
    }
    db->clear_cache();
    db->exec(std::format("DROP SCHEMA {} CASCADE", schema));
    std::cout << (suite::failures ? "PostgreSQL suite FAILED\n" : "PostgreSQL suite passed\n");
    return suite::failures ? 1 : 0;
}
