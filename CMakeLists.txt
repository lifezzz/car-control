cmake_minimum_required(VERSION 3.13)

project(four_wheel_car_sim
        DESCRIPTION "Four wheel car control simulator"
        LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(car
        src/main.c
        src/car.c
        src/command.c
        src/motion.c
        src/cli.c)

target_include_directories(car PRIVATE include)

if(MSVC)
    target_compile_options(car PRIVATE /W4 /utf-8)
else()
    target_compile_options(car PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Regression tests, driven by the same python script as "make test".
enable_testing()
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME regression
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.py
                     --bin $<TARGET_FILE:car>)
endif()
