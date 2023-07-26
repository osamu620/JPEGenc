cmake_minimum_required(VERSION 3.18)
project (JPEGenc LANGUAGES C CXX DESCRIPTION "JPEG-1 encoder")
option(BUILD_SHARED_LIBS "Shared Libraries" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_PREFIX_PATH "./include") # for include
add_definitions("-DBUILD_TESTING=OFF")
#add_definitions("-DHWY_COMPILE_ONLY_SCALAR") # uncomment to build non-simd code

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/highway)
add_library(jpegenc ${SOURCES})
target_include_directories(
		jpegenc
		PRIVATE lib include thirdparty/hwy
)
target_include_directories(jpegenc INTERFACE include)
target_link_libraries(jpegenc PRIVATE hwy)
add_subdirectory(lib)
set_target_properties(jpegenc PROPERTIES OUTPUT_NAME
		$<IF:$<CONFIG:Debug>,jpegenc_d,jpegenc_R>)

add_executable(jpenc)
add_subdirectory(apps)
set_target_properties(
		jpenc
		PROPERTIES OUTPUT_NAME
		$<IF:$<CONFIG:Debug>,jpenc_dbg,jpenc>)

#add_executable(hwytest apps/hwytest.cpp)
#target_include_directories(hwytest PRIVATE thirdparty/hwy)
#target_link_libraries(hwytest PRIVATE hwy)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
	set(CMAKE_CXX_FLAGS "-Wall -Wextra -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer ${COMPILER_HOST_FLAGS}")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=address")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3")
	set(CMAKE_CXX_FLAGS_RelWithDebInfo "${CMAKE_CXX_FLAGS} -O2 -g")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC") # MSVC
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd5051 /EHsc /D \"_CRT_SECURE_NO_WARNINGS\"")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} /Od /DDEBUG /ZI")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} /Ox")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} /O2 /ZI")
endif()

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^[aA][rR][mM]64$|^[aA][aA][rR][cC][hH]64$") # aarch64
	if(APPLE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mtune=native -mcpu=apple-m1")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mtune=cortex-a72 -march=armv8-a")
	endif()
	if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flax-vector-conversions")
	endif()
else()
	if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mtune=native -march=native") # x86_64
	endif()
endif()







