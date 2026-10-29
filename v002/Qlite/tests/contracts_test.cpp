// Death-test for Qlite's contracts (on SQLite, which needs no server): reading a column with
// no current row or past the row, and binding past the placeholders, must terminate; the
// same calls used rightly must not.

#include <Qlite/v002/Sqlite.hpp>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using Qlite::v002::Sqlite;

// Run `fn` in a forked child; true iff the child did not exit cleanly (a contract fired).
template <class Fn>
static bool child_terminates(Fn fn) {
    std::fflush(nullptr);
    const pid_t pid = fork();
    if (pid < 0) { std::perror("fork"); std::_Exit(2); }
    if (pid == 0) {
        fn();
        std::_Exit(0);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) { /* retry on EINTR */ }
    return !(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static Sqlite table() {
    Sqlite db(":memory:");
    db.exec("CREATE TABLE t (id INTEGER, name TEXT)");
    db.exec("INSERT INTO t VALUES (?, ?)", 1, std::string("one"));
    return db;
}

int main() {
    int failures = 0;
    auto expect = [&](bool terminates, const char* what, auto fn) {
        if (child_terminates(fn) != terminates) {
            std::cerr << "FAIL: " << what << (terminates ? " did NOT terminate\n" : " terminated\n");
            ++failures;
        }
    };

    expect(false, "a query used rightly", [] {
        auto db = table();
        auto st = db.prepare("SELECT id, name FROM t WHERE id = ?");
        st.bind(1);
        std::int64_t id = 0;
        std::string name;
        if (!st.step()) std::_Exit(3);
        st.get(id, name);
        (void)st.get_at<std::string>(1);
        auto ins = db.prepare("INSERT INTO t VALUES (?, ?)");
        ins.bind_int64(1, 2);
        ins.bind_null(2);
    });
    expect(true, "get before step()", [] {
        auto db = table();
        auto st = db.prepare("SELECT id FROM t");
        std::int64_t id = 0;
        st.get(id);
    });
    expect(true, "get after step() returned false", [] {
        auto db = table();
        auto st = db.prepare("SELECT id FROM t");
        while (st.step()) {}
        std::int64_t id = 0;
        st.get(id);
    });
    expect(true, "get_at past the row", [] {
        auto db = table();
        auto st = db.prepare("SELECT id FROM t");
        (void)st.step();
        (void)st.get_at<std::int64_t>(1);
    });
    expect(true, "more values than placeholders", [] {
        auto db = table();
        auto st = db.prepare("SELECT id FROM t WHERE id = ?");
        st.bind(1, 2);
    });
    expect(true, "bind_text past the placeholders", [] {
        auto db = table();
        auto st = db.prepare("SELECT id FROM t WHERE id = ?");
        st.bind_text(2, "x");
    });

    if (failures == 0)
        std::cout << "Qlite contracts — misuse terminates, the right use does not (native="
                  << QLITE_CONTRACTS_NATIVE << ")\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
