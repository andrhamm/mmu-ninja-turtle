add_executable(nt_tmc2209_tests ${CMAKE_SOURCE_DIR}/src/nt/tmc2209.cpp test_tmc2209.cpp)
add_catch_test(nt_tmc2209_tests)
