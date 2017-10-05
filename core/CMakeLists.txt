cmake_minimum_required(VERSION 3.2)

project(glitch-core)

add_library(glitch-core STATIC glitch.c)
set_target_properties(glitch-core PROPERTIES C_STANDARD 99)

enable_testing()

add_executable(glitch_test glitch_test.c)
set_target_properties(glitch_test PROPERTIES C_STANDARD 99)
target_link_libraries(glitch_test m)
add_test(glitch_test glitch_test)
