add_executable(nt_tmc2209_tests ${CMAKE_SOURCE_DIR}/src/nt/tmc2209.cpp test_tmc2209.cpp)
add_catch_test(nt_tmc2209_tests)

add_executable(nt_flash_journal_tests ${CMAKE_SOURCE_DIR}/src/nt/flash_journal.cpp test_flash_journal.cpp)
add_catch_test(nt_flash_journal_tests)
