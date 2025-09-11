add_library(order_generator INTERFACE)
target_include_directories(order_generator INTERFACE include)

target_link_libraries(order_generator INTERFACE 
	common
)