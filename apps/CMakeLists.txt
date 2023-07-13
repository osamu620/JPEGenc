cmake_policy(SET CMP0076 NEW)
target_sources(jpenc
        PRIVATE
        main_enc.cpp
        )
target_include_directories(jpenc PUBLIC include)
target_link_libraries(jpenc PUBLIC jpegenc)