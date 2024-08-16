# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
   set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(PICO_SDK_PATH ${USERHOME}/.pico-sdk/sdk/2.0.0)
set(PICO_TOOLCHAIN_PATH ${USERHOME}/.pico-sdk/toolchain/13_2_Rel1)
set(pioasm_HINT ${USERHOME}/.pico-sdk/tools/2.0.0/pioasm)
if(EXISTS ${pioasm_HINT})
    set(pioasm_DIR ${pioasm_HINT})
endif()
set(picotool_HINT ${USERHOME}/.pico-sdk/picotool/2.0.0/picotool)
if(EXISTS ${picotool_HINT})
    set(picotool_DIR ${picotool_HINT})
endif()
if(PICO_TOOLCHAIN_PATH MATCHES "RISCV")
    set(PICO_PLATFORM rp2350-riscv CACHE STRING "Pico Platform")
    if(PICO_TOOLCHAIN_PATH MATCHES "COREV")
        set(PICO_COMPILER pico_riscv_gcc_zcb_zcmp)
    endif()
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(a800v1 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()


# Add executable. Default name is the project name, version 0.1
add_executable(a800v1   
rom.cc
ewoz.cc
core1_main.cc
system.cc
mos65C02.cc
mos65C02_pio.cc
m6520.cc
menu.cc
memory.cc
antic.cc
gtia.cc
pokey.cc
atarixl.cc
atarixe.cc
ataribas.cc
atbasic158.cc
donkeykong.cc
pacman.cc
boulderdash.cc
miner_2049.cc
pengo.cc
spaceinvaders.cc
usb_hid.cc
emu_m6502.cc
memcpy.cc
#sound.cc
sound_pio.cc
salt205.cc
mspacman_rom.cc
starraiders.cc
main.cc
)

pico_set_program_name(a800v1 "a800v1")
pico_set_program_version(a800v1 "0.1")

# Generate PIO header
#pico_generate_pio_header(a800v1 ${CMAKE_CURRENT_LIST_DIR}/blink.pio)
pico_generate_pio_header(a800v1 ${CMAKE_CURRENT_LIST_DIR}/sm0_memory_emulation_with_clock.pio)
pico_generate_pio_header(a800v1 ${CMAKE_CURRENT_LIST_DIR}/sound_pio.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(a800v1 1)
pico_enable_stdio_usb(a800v1 0)

# Add the standard library to the build
target_link_libraries(a800v1
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(a800v1 PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# add compile definitions
target_compile_definitions(a800v1 PUBLIC
        #PARAM_ASSERTIONS_ENABLE_ALL=1
        PICO_DEFAULT_UART=0
        PICO_DEFAULT_UART_TX_PIN=28
        PICO_DEFAULT_UART_RX_PIN=29
        PICO_DEFAULT_UART_BAUD_RATE=115200
        #PICO_DEFAULT_UART_BAUD_RATE=921600
        PICO_STACK_SIZE=0x300
        PICO_DIVIDER_IN_RAM=1
        DVI_DEFAULT_SERIAL_CONFIG=pico_neo6502_cfg
        #DVI_MONOCHROME_TMDS=1
        DVI_VERTICAL_REPEAT=2
        #TMDS_FULLRES_NO_INTERP_SAVE=1
        DVI_N_TMDS_BUFFERS=5
        DVI_SYMBOLS_PER_WORD=2
        TMDS_ENCODE_UNROLL=1
        PLAYERMISSILE
        #PAL_COLORS
        NTSC_COLORS
        #EMU_M6502=1
        #SALT=1 

        #OPTION_SOUND_INTERRUPTS

        OPTION_MODE4_EXPERIMENTAL
        OPTION_MODE6_EXPERIMENTAL

        OPTION_6502_PIO

        #OPTION_TMDS
) 
# Add any user requested libraries
target_link_libraries(a800v1 
        pico_multicore
        pico_mem_ops
        pico_util
        pico_time
        hardware_divider
        hardware_dma
        hardware_gpio
        hardware_pio
        hardware_interp
        hardware_timer
        hardware_clocks
        hardware_pwm
        tinyusb_host
        tinyusb_board
        libdvi
        )

add_compile_options(
    -Wwrite-strings
    -Wall
    -Wno-unused-function
    -O2
    -mcpu=cortex-m0plus
    #-tune=cortex-m0plus
    #-funroll-loops
    # -mlong-calls
 )


pico_add_extra_outputs(a800v1)

add_subdirectory(PicoDVI/software/libdvi)

#pico_set_binary_type(a800v1 copy_to_ram)

