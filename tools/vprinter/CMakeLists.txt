# vprinter: Prusa Buddy's printer-side MMU protocol logic (vendored, v6.10.1) as a host test harness.
set(BUDDY ${CMAKE_SOURCE_DIR}/third_party/buddy)
add_executable(
  vprinter main.cpp host_stubs.cpp ${BUDDY}/lib/Marlin/Marlin/src/feature/prusa/MMU2/protocol_logic.cpp
           ${BUDDY}/lib/Prusa-Firmware-MMU/src/modules/protocol.cpp ${BUDDY}/lib/Prusa-Firmware-MMU/src/modules/crc.cpp
  )
target_compile_definitions(vprinter PRIVATE UNITTEST)
target_include_directories(
  vprinter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}
                   ${BUDDY}/lib/Marlin/Marlin/src/feature/prusa/MMU2 ${BUDDY}/src/common/utils
  )
set_target_properties(vprinter PROPERTIES CXX_STANDARD 20) # Buddy code uses C++20 features

set(LOOP ${CMAKE_CURRENT_SOURCE_DIR}/loop_test.sh $<TARGET_FILE:vmmu> $<TARGET_FILE:vprinter>)
add_test(NAME loop::lineup COMMAND ${LOOP} --scenario lineup --gate-ms 10)
add_test(NAME loop::idle_30s COMMAND ${LOOP} --scenario idle:30 --gate-ms 10)
add_test(NAME loop::toolchange_x5 COMMAND ${LOOP} --scenario tc:5 --gate-ms 10)
add_test(NAME loop::error_retry COMMAND ${LOOP} --scenario tc-retry --gate-ms 10)
set_tests_properties(loop::error_retry PROPERTIES ENVIRONMENT "VMMU_ARGS=--fault finda-stuck-off:6000")
set_tests_properties(loop::idle_30s loop::toolchange_x5 loop::error_retry PROPERTIES TIMEOUT 300)
