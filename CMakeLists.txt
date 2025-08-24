cmake_minimum_required(VERSION 3.25)

project(hft_playpen 
    LANGUAGES CXX
    VERSION 0.1.0
    DESCRIPTION "High-frequency trading components playground"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC flags
if(MSVC)
    add_compile_options(/permissive- /W3)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/Od /Zi /MDd)
        add_link_options(/DEBUG)
    else()
        add_compile_options(/O2 /DNDEBUG /MD)
    endif()
endif()

# External dependencies
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

enable_testing()

# Components
add_subdirectory(ring_buffer)
add_subdirectory(slab_alloc)