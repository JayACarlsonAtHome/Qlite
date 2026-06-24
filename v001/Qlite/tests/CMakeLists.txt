add_executable(qlite_v001_test v001_test.cpp)
target_link_libraries(qlite_v001_test PRIVATE Qlite::Qlite)
add_test(NAME qlite_v001_test COMMAND qlite_v001_test)
