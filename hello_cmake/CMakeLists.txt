cmake_minimum_required(VERSION 3.20)

project(HelloCMake
VERSION 1.0.0
LANGUAGES CXX
)

find_package(OpenSSL REQUIRED)

message(CMAKE_CXX_COMPILER_ID = ${CMAKE_CXX_COMPILER_ID})
add_executable(hello src/main.cc src/hello.cc)
target_include_directories(hello PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(hello PRIVATE HELLO_TEST=1)
target_compile_features(hello PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILE_ID MATCHES "GNU|Clang")
	target_compile_options(hello PRIVATE -Wall -Wextra)
elseif("MSVS")
	target_compile_options(hello PRIVATE /W4)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	message(STATUS "Building for Linux")
endif()

foreach(src IN ITEMS a.c b.c c.c)
	message(STATUS "Source: ${src}")
endforeach()
