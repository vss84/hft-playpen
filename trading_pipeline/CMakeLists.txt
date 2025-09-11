add_executable(trading_pipeline 
    src/trading_pipeline.cpp
)

target_include_directories(trading_pipeline 
    PRIVATE include
    PRIVATE src
)

target_link_libraries(trading_pipeline PRIVATE 
	order_generator
	common 
	order_parser 
	ring_buffer 
	slab_alloc 
	matching_engine 
	logger
)