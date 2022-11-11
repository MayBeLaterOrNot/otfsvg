cmake_minimum_required(VERSION 3.5)

project(otfsvg LANGUAGES C)

add_library(otfsvg STATIC otfsvg.h otfsvg.c)

target_include_directories(otfsvg PUBLIC "${CMAKE_CURRENT_LIST_DIR}")

add_executable(otfsvg-dump otfsvg-dump.c)
target_link_libraries(otfsvg-dump otfsvg m)
