#include <cstdlib>
#include <string>
#include <cstdint>

import Qlite.v001;

// Functional module smoke. This consumer is an ordinary TU (textual std includes)
// that imports the module and EXERCISES the exported symbols — Sqlite, Statement,
// bind/column, SqliteError, plus the new transaction guard — end to end against an
// in-memory database. Driving a real round-trip proves both module front-ends
// actually export working definitions (textual export-namespace path for clang AND
// the import-std path for the g++ pilot).
int main() {
    using namespace Qlite::v001;
    try {
        Sqlite db(":memory:");
        db.exec("CREATE TABLE t(id INTEGER, name TEXT);");

        {
            auto tx = db.transaction();
            db.exec("INSERT INTO t VALUES(?, ?);", std::int64_t{42}, std::string("alpha"));
            tx.commit();
        }

        auto st = db.prepare("SELECT id, name FROM t WHERE id = ?;");
        st.bind(std::int64_t{42});
        if (!st.step()) return EXIT_FAILURE;

        std::int64_t id = 0;
        std::string name;
        st.get(id, name);
        if (id != 42 || name != "alpha") return EXIT_FAILURE;
    } catch (const SqliteError&) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
