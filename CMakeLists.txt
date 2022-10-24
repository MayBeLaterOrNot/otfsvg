cmake_minimum_required(VERSION 3.5)

project(otfsvg LANGUAGES C)

add_library(otfsvg STATIC otfsvg.h otfsvg-private.h otfsvg-geometry.c otfsvg-document.c)

add_executable(otfsvg-dump otfsvg-dump.c)
target_link_libraries(otfsvg-dump otfsvg m)
