add_library(logger INTERFACE)
target_include_directories(logger INTERFACE include)

# Tests
add_executable(logger_test tests/logger_test.cpp)
target_link_libraries(logger_test PRIVATE logger gtest_main)
add_test(NAME logger_test COMMAND logger_test)

# Benchmarks
#add_executable(logger_benchmark benchmarks/logger_benchmark.cpp)
#target_link_libraries(logger_benchmark PRIVATE logger benchmark::benchmark_main)