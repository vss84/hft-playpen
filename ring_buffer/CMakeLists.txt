add_library(ring_buffer INTERFACE)
target_include_directories(ring_buffer INTERFACE include)

# Tests
add_executable(ring_buffer_test tests/test_ring_buffer.cpp)
target_link_libraries(ring_buffer_test PRIVATE ring_buffer gtest_main)
add_test(NAME ring_buffer_test COMMAND ring_buffer_test)

# Benchmarks
add_executable(ring_buffer_benchmark benchmarks/bench_ring_buffer.cpp)
target_link_libraries(ring_buffer_benchmark PRIVATE ring_buffer benchmark::benchmark_main)