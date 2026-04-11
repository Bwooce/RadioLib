cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(RadioLibZephyrExample)

# Include RadioLib headers
target_include_directories(app PRIVATE ../../../src)

# Collect all RadioLib source files
file(GLOB_RECURSE RADIOLIB_SRC ../../../src/*.cpp)

# Add to the app target
target_sources(app PRIVATE main.cpp ${RADIOLIB_SRC})
