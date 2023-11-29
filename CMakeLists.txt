cmake_minimum_required(VERSION 3.19)
project(batari-Basic C)
set(CMAKE_C_STANDARD 90)

#build the executable
add_executable(batari-Basic 2600bas.c
        linker.c linker.h
        lexer.c lexer.h
        statements.c statements.h
        keywords.c keywords.h
        lib_gfx.c lib_gfx.h
        lib_dpcplus.c lib_dpcplus.h)

add_executable(postprocess postprocess.c linker.c linker.h)

add_executable(bbfilter bbfilter.c)