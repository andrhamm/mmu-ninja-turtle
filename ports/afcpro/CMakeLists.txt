# AFC-Pro (STM32H723, Cortex-M7) port — Phase 6. Standalone project (the root CMakeLists is AVR/host oriented).
# Step 1: compile the portable firmware core (application, logic, modules) for Cortex-M7 to find AVR-only code.
cmake_minimum_required(VERSION 3.19)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
project(ninja_turtle_afcpro C CXX ASM)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../version.cmake)
set(MCU_FLAGS -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard)

file(GLOB LOGIC ${SRC}/logic/*.cpp)
add_library(
  nt_core STATIC
  ${SRC}/application.cpp ${SRC}/registers.cpp
  ${SRC}/modules/buttons.cpp ${SRC}/modules/debouncer.cpp ${SRC}/modules/finda.cpp ${SRC}/modules/fsensor.cpp
  ${SRC}/modules/globals.cpp ${SRC}/modules/idler.cpp ${SRC}/modules/leds.cpp ${SRC}/modules/movable_base.cpp
  ${SRC}/modules/permanent_storage.cpp ${SRC}/modules/protocol.cpp ${SRC}/modules/pulley.cpp ${SRC}/modules/selector.cpp
  ${SRC}/modules/user_input.cpp ${SRC}/modules/crc.cpp
  ${LOGIC}
  )
target_include_directories(nt_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${SRC}) # not src/modules: its math.h would shadow <math.h> under newlib
target_compile_options(nt_core PRIVATE ${MCU_FLAGS} -Os -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti
  $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>)
target_compile_definitions(nt_core PRIVATE PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR} PROJECT_VERSION_REV=${PROJECT_VERSION_REV} PROJECT_BUILD_NUMBER=0
  FW_VERSION_STR="${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_REV}" FW_VERSION_FULL_STR="nt"
  FW_VERSION_SUFFIX_STR="" FW_VERSION_SUFFIX_SHORT_STR="")

# Step 2: firmware image for the AFC-Pro (Box Turtle). HAL: src/hal/stm32 + hal_afcpro.cpp; CMSIS in third_party/cmsis.
set(CMSIS ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/cmsis)
set(NT_DEFS NT_STM32 NT_BOXTURTLE STM32H723xx PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR} PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  PROJECT_VERSION_REV=${PROJECT_VERSION_REV} PROJECT_BUILD_NUMBER=0
  FW_VERSION_STR="${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_REV}" FW_VERSION_FULL_STR="nt"
  FW_VERSION_SUFFIX_STR="" FW_VERSION_SUFFIX_SHORT_STR="")
add_executable(
  nt_afcpro.elf
  main.cpp hal_afcpro.cpp tmc_uart.cpp ${SRC}/nt/virtual_axes.cpp ${SRC}/nt/tmc2209.cpp ${SRC}/nt/flash_journal.cpp
  ${SRC}/application.cpp ${SRC}/registers.cpp
  ${SRC}/modules/buttons.cpp ${SRC}/modules/debouncer.cpp ${SRC}/modules/finda.cpp ${SRC}/modules/fsensor.cpp
  ${SRC}/modules/globals.cpp ${SRC}/modules/idler.cpp ${SRC}/modules/leds.cpp ${SRC}/modules/movable_base.cpp
  ${SRC}/modules/motion.cpp ${SRC}/modules/pulse_gen.cpp ${SRC}/modules/speed_table.cpp
  ${SRC}/modules/permanent_storage.cpp ${SRC}/modules/protocol.cpp ${SRC}/modules/pulley.cpp ${SRC}/modules/selector.cpp
  ${SRC}/modules/user_input.cpp ${SRC}/modules/serial.cpp
  ${LOGIC}
  ${CMSIS}/device-h7/Source/startup_stm32h723xx.s ${CMSIS}/device-h7/Source/system_stm32h7xx.c
  )
target_include_directories(nt_afcpro.elf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${SRC} ${CMSIS}/device-h7/Include ${CMSIS}/core/Include)
target_compile_definitions(nt_afcpro.elf PRIVATE ${NT_DEFS})
target_compile_options(nt_afcpro.elf PRIVATE ${MCU_FLAGS} -Os -g -ffunction-sections -fdata-sections -Wall
  $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics>)
target_link_options(nt_afcpro.elf PRIVATE ${MCU_FLAGS} -T${CMAKE_CURRENT_SOURCE_DIR}/STM32H723.ld -Wl,--gc-sections
  -Wl,-Map=nt_afcpro.map --specs=nano.specs --specs=nosys.specs -Wl,--print-memory-usage)
add_custom_command(TARGET nt_afcpro.elf POST_BUILD COMMAND arm-none-eabi-objcopy -O binary nt_afcpro.elf nt_afcpro.bin)
