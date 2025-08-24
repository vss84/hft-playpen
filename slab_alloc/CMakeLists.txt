add_library(slab_alloc INTERFACE)
target_include_directories(slab_alloc INTERFACE include)

# Tests
add_executable(slab_alloc_test tests/test_slab_alloc.cpp)
target_link_libraries(slab_alloc_test PRIVATE slab_alloc gtest_main)
add_test(NAME slab_alloc_test COMMAND slab_alloc_test)

# Benchmarks
add_executable(slab_alloc_benchmark benchmarks/bench_slab_alloc.cpp)
target_link_libraries(slab_alloc_benchmark PRIVATE slab_alloc benchmark::benchmark_main)