cmake_policy(SET CMP0076 NEW)
target_sources(jpegenc
        PRIVATE
        main_enc.cpp
        )
target_include_directories(jpegenc PUBLIC include)
target_link_libraries(jpegenc PUBLIC libjpegenc)