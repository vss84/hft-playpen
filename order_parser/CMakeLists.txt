add_library(order_parser INTERFACE)
target_include_directories(order_parser INTERFACE include)

target_link_libraries(order_parser INTERFACE 
	common
	protocol
)

add_executable(order_parser_test tests/test_order_parser.cpp)
target_link_libraries(order_parser_test PRIVATE order_parser gtest_main)
add_test(NAME order_parser_test COMMAND order_parser_test)