# DS5Dongle-ESP — ESP32-S31 firmware (migrated from Raspberry Pi Pico 2W)
#
# Build:
#   idf.py set-target esp32s31     # confirm exact target via `idf.py --list-targets`
#   idf.py build flash monitor
#
# ESP32-S31 is new — its target string and some Kconfig symbols may differ
# between ESP-IDF releases. See docs/MIGRATION.md.
cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(ds5dongle_esp32s31)
