add_executable(qlite_v002_test v002_test.cpp)
target_link_libraries(qlite_v002_test PRIVATE Qlite::Qlite)
add_test(NAME qlite_v002_test COMMAND qlite_v002_test)
