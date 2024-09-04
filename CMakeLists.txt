cmake_minimum_required(VERSION 3.25)
set(CMAKE_C_STANDARD 23)

project(jellyc)

find_package(OpenMP)

set(SOURCE
    src/arena.c
    src/data/tir.c
    src/diagnostic.c
    src/float.c
    src/gen.c
    src/gen-llvm.c
    src/hash.c
    src/lex.c
    src/main.c
    src/parse.c
    src/print.c
    src/role-analysis.c
    src/tir-analysis.c
    src/tir2mir.c
    src/type-analysis.c
    src/util.c
)

set_source_files_properties(
    ${SOURCE}
    PROPERTIES
    COMPILE_FLAGS "-pedantic -Wall -Wextra -Wmissing-field-initializers -Werror=shadow -Werror=return-type -Werror=incompatible-pointer-types"
)

if (OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

add_executable(jellyc ${SOURCE})

target_include_directories(jellyc PRIVATE src)
target_link_libraries(jellyc m)
