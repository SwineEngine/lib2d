cmake_minimum_required(VERSION 2.8.7)
project(lib2d-examples)

add_definitions(-std=c99 -Wall -pedantic)

configure_file(rounded_square.png ${CMAKE_CURRENT_BINARY_DIR}/rounded_square.png COPYONLY)
add_executable(lotsofsprites lotsofsprites.c)
target_link_libraries(lotsofsprites lib2d SDL2)
