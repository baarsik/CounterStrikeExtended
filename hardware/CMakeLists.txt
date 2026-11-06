cmake_minimum_required(VERSION 4.2)
project(CounterStrikeExtendedHardware LANGUAGES NONE)
set(CSE_HARDWARE_ONLY ON CACHE BOOL "Hardware engine entry point" FORCE)
include("${CMAKE_CURRENT_LIST_DIR}/../CMakeLists.txt")
