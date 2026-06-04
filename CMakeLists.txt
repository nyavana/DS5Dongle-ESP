# DS5Dongle-ESP — ESP32-S31 firmware (migrated from Raspberry Pi Pico 2W)
#
# Build (esp32s31 is a PREVIEW target in ESP-IDF master — note --preview):
#   idf.py --preview set-target esp32s31
#   idf.py build flash monitor
#
# ESP32-S31 support currently lives only in ESP-IDF master; target string and
# some Kconfig symbols may still shift. See docs/MIGRATION.md.
cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(ds5dongle_esp32s31)
