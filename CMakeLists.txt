cmake_minimum_required(VERSION 3.0)
project (TEST LANGUAGES CXX DESCRIPTION "JPEG-1 encoder")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-Wall -mtune=native -march=native")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=address")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2")
	set(CMAKE_CXX_FLAGS_RelWithDebInfo "${CMAKE_CXX_FLAGS} -O2 -g")
endif()

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^[aA][rR][mM]64$|^[aA][aA][rR][cC][hH]64$") # aarch64
	if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flax-vector-conversions")
	endif()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(jpegenc main.cpp jpgheaders.cpp color.cpp dct.cpp quantization.cpp block_coding.cpp)

