add_library(protocol INTERFACE)
target_include_directories(protocol INTERFACE include)

add_executable(protocol_test tests/test_protocol.cpp)
target_link_libraries(protocol_test PRIVATE protocol gtest_main)
add_test(NAME protocol_test COMMAND protocol_test)