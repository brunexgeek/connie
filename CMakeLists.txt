cmake_minimum_required(VERSION 3.18)
project(connie)


set(CONNIE_MAJOR_VERSION 0)
set(CONNIE_MINOR_VERSION 1)
set(CONNIE_PATCH_VERSION 0)

if (CMAKE_BUILD_TYPE STREQUAL "")
    message(STATUS "No build type selected, default to 'Release'")
    set(CMAKE_BUILD_TYPE "Release")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CONNIE_BUILD_DEBUG 1)
else()
    set(CONNIE_BUILD_RELEASE 1)
endif()

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

if (UNIX)
    set(ENABLE_PROFILING OFF CACHE BOOL "")
    set(ENABLE_SANITIZER OFF CACHE BOOL "")
    set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -std=c11 -fPIC -Werror=incompatible-pointer-types -Werror=implicit-function-declaration -Werror=return-type -Werror=int-conversion -Wunused-function -Wunused-variable -Wmissing-declarations -Wmissing-prototypes -Wl,--no-undefined -Wconversion")

    if (ENABLE_PROFILING)
        set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -pg")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O3")
    else()
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -O3")
    endif()

    set(CMAKE_C_FLAGS_DEBUG     "${CMAKE_C_FLAGS_DEBUG} -g -O0")

    if (ENABLE_SANITIZER)
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fsanitize=address")
    endif()
endif()

add_library(libconnie_static STATIC "connie.c")
target_include_directories(libconnie_static PUBLIC "include")
set_target_properties(libconnie_static PROPERTIES
    OUTPUT_NAME "connie"
    VERSION "${CONNIE_MAJOR_VERSION}.${CONNIE_MINOR_VERSION}.${CONNIE_PATCH_VERSION}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_library(libconnie SHARED "connie.c")
target_include_directories(libconnie PUBLIC "include")
set_target_properties(libconnie PROPERTIES
    OUTPUT_NAME "connie"
    VERSION "${CONNIE_MAJOR_VERSION}.${CONNIE_MINOR_VERSION}.${CONNIE_PATCH_VERSION}"
    SOVERSION "${CONNIE_MAJOR_VERSION}.${CONNIE_MINOR_VERSION}.${CONNIE_PATCH_VERSION}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" )

add_executable(example1 "example1.c")
target_link_libraries(example1 libconnie_static)
target_compile_definitions(example1 PRIVATE _DEFAULT_SOURCE)
set_target_properties(example1 PROPERTIES
    OUTPUT_NAME "example1"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" )
