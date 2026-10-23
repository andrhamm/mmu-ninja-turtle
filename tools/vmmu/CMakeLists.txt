# vmmu: the real MMU application/logic running on the host, talking MMU3 protocol on a serial device or pty.
# Motors, sensors and EEPROM are simulated (stubs from tests/unit + physics.cpp).
set(STUBS ${CMAKE_SOURCE_DIR}/tests/unit/modules/stubs)
set(LSTUBS ${CMAKE_SOURCE_DIR}/tests/unit/logic/stubs)

add_executable(
  vmmu
  main.cpp
  posix_serial.cpp
  physics.cpp
  ${CMAKE_SOURCE_DIR}/src/registers.cpp
  ${CMAKE_SOURCE_DIR}/src/application.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/buttons.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/leds.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/globals.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/finda.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/debouncer.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/fsensor.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/idler.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/movable_base.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/permanent_storage.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/protocol.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/pulley.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/selector.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/user_input.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/pulse_gen.cpp
  ${CMAKE_SOURCE_DIR}/src/modules/voltage.cpp
  ${STUBS}/stub_adc.cpp
  ${STUBS}/stub_cpu.cpp
  eeprom_host.cpp
  ${STUBS}/stub_gpio.cpp
  ${STUBS}/stub_panic.cpp
  ${STUBS}/stub_shr16.cpp
  ${STUBS}/stub_timebase.cpp
  ${STUBS}/stub_tmc2130.cpp
  motion_sim.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/no_command.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/command_base.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/cut_filament.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/load_filament.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/move_selector.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/eject_filament.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/tool_change.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/unload_filament.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/home.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/set_mode.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/start_up.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/feed_to_finda.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/feed_to_bondtech.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/retract_from_finda.cpp
  ${CMAKE_SOURCE_DIR}/src/logic/unload_to_finda.cpp
  )
target_compile_definitions(vmmu PRIVATE UNITTEST)
target_include_directories(
  vmmu PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/modules ${CMAKE_SOURCE_DIR}/src/hal ${STUBS} ${LSTUBS}
  )
if(NOT APPLE)
  target_link_libraries(vmmu PRIVATE util) # openpty
endif()

add_test(NAME vmmu::lineup COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/smoke_test.sh $<TARGET_FILE:vmmu>)
add_test(NAME vmmu::load_slot1 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/smoke_test.sh $<TARGET_FILE:vmmu> load)
add_test(NAME vmmu::fault_finda_stuck_off COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/smoke_test.sh $<TARGET_FILE:vmmu> tc-error --fault finda-stuck-off)
add_test(NAME vmmu::fault_selector_home COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/smoke_test.sh $<TARGET_FILE:vmmu> load-error --fault selector-home-fail)
set_tests_properties(vmmu::load_slot1 vmmu::fault_finda_stuck_off vmmu::fault_selector_home PROPERTIES TIMEOUT 120)
