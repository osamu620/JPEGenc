cmake_minimum_required(VERSION 3.18)
project (JPEGenc LANGUAGES C CXX DESCRIPTION "JPEG-1 encoder")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(jpegenc main.cpp jpgheaders.cpp color.cpp dct.cpp quantization.cpp block_coding.cpp)

#include(DetectHostArch.cmake)
#detect_host_arch()
#string (REPLACE ";" " " COMPILER_HOST_FLAGS "${HOST_FLAGS}")
#message(STATUS "${CMAKE_CXX_COMPILER_ID} flags: ${COMPILER_HOST_FLAGS}")

set(CMAKE_CXX_FLAGS "-Wall -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer ${COMPILER_HOST_FLAGS}")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=address")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2")
	set(CMAKE_CXX_FLAGS_RelWithDebInfo "${CMAKE_CXX_FLAGS} -O2 -g")
endif()

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^[aA][rR][mM]64$|^[aA][aA][rR][cC][hH]64$") # aarch64
	target_compile_definitions(jpegenc PUBLIC "JPEG_USE_NEON")
	if(APPLE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mtune=native -mcpu=apple-m1")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mtune=cortex-a72 -march=armv8-a")
	endif()
	if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flax-vector-conversions")
	endif()
else()
	target_compile_definitions(jpegenc PUBLIC "JPEG_USE_AVX2")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mtune=native -march=native") # x86_64
endif()





