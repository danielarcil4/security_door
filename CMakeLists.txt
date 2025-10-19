# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(security_door C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(security_door main.c Src/matrix_keyboard.c Src/buzzer.c)

pico_generate_pio_header(security_door ${CMAKE_CURRENT_LIST_DIR}/matrix_keyboard.pio ${CMAKE_CURRENT_LIST_DIR}/buzzer_square.pio)

pico_set_program_name(security_door "security_door")
pico_set_program_version(security_door "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(security_door 0)
pico_enable_stdio_usb(security_door 1)

# Add the standard library to the build
target_link_libraries(security_door
        pico_stdlib
        hardware_pio
        hardware_pwm
        )

# Add the standard include files to the build
target_include_directories(security_door PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(security_door)

