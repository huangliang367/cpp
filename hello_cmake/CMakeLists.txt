cmake_minimum_required(VERSION 3.20)

project(HelloCMake
		VERSION 1.0.0
		LANGUAGES CXX
	)

add_executable(hello src/main.cc src/hello.cc)
target_include_directories(hello PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(hello PRIVATE HELLO_TEST=1)