add_executable(qlite_v001_test v001_test.cpp)
target_link_libraries(qlite_v001_test PRIVATE Qlite::Qlite)
add_test(NAME qlite_v001_test COMMAND qlite_v001_test)

# The one suite (suite.hpp) on every database this machine has. PostgreSQL and MariaDB
# report skipped (exit 77) where no server answers.
add_executable(qlite_v001_sqlite_suite sqlite_suite_test.cpp)
target_link_libraries(qlite_v001_sqlite_suite PRIVATE Qlite::Qlite)
add_test(NAME qlite_v001_sqlite_suite COMMAND qlite_v001_sqlite_suite)

find_package(PostgreSQL QUIET)
if(TARGET PostgreSQL::PostgreSQL)
    add_executable(qlite_v001_postgres_suite postgres_suite_test.cpp)
    target_link_libraries(qlite_v001_postgres_suite PRIVATE Qlite::Qlite PostgreSQL::PostgreSQL)
    add_test(NAME qlite_v001_postgres_suite COMMAND qlite_v001_postgres_suite)
    set_tests_properties(qlite_v001_postgres_suite PROPERTIES SKIP_RETURN_CODE 77)
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(QLITE_MARIADB QUIET IMPORTED_TARGET libmariadb)
endif()
if(TARGET PkgConfig::QLITE_MARIADB)
    add_executable(qlite_v001_mariadb_suite mariadb_suite_test.cpp)
    target_link_libraries(qlite_v001_mariadb_suite PRIVATE Qlite::Qlite PkgConfig::QLITE_MARIADB)
    add_test(NAME qlite_v001_mariadb_suite COMMAND qlite_v001_mariadb_suite)
    set_tests_properties(qlite_v001_mariadb_suite PROPERTIES SKIP_RETURN_CODE 77)
endif()
