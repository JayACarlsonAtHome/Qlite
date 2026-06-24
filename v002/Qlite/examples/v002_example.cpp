#include <Qlite/v002.hpp>

#include <iostream>
#include <optional>
#include <string>

// Minimal Qlite::v002 demo: open, create, insert (variadic bind), and iterate
// with a RAII transaction guard around the writes.
int main() {
    try {
        Qlite::v002::Sqlite db(":memory:");

        db.exec("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)");

        {
            auto tx = db.transaction();                 // BEGIN; auto-rollback unless committed
            db.exec("INSERT INTO users (name, age) VALUES (?, ?)", std::string("Alice"), 30);
            db.exec("INSERT INTO users (name, age) VALUES (?, ?)", std::string("Bob"),
                    std::optional<int>{});               // NULL age
            tx.commit();
        }

        auto stmt = db.prepare("SELECT id, name, age FROM users ORDER BY id");
        while (stmt.step()) {
            std::int64_t id;
            std::string name;
            std::optional<std::int64_t> age;
            stmt.get(id, name, age);
            std::cout << id << ": " << name
                      << " (age=" << (age ? std::to_string(*age) : "null") << ")\n";
        }
    } catch (const Qlite::v002::SqliteError& e) {
        std::cerr << "SQLite error: " << e.what() << " (code " << e.code() << ")\n";
        return 1;
    }
    return 0;
}
