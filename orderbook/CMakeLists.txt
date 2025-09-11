add_library(orderbook INTERFACE)
target_include_directories(orderbook INTERFACE include)

target_link_libraries(orderbook INTERFACE common)

add_executable(orderbook_test tests/test_orderbook.cpp)
target_link_libraries(orderbook_test PRIVATE orderbook gtest_main)
add_test(NAME orderbook_test COMMAND orderbook_test)