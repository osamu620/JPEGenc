cmake_policy(SET CMP0076 NEW)
target_sources(jpegenc
        PRIVATE
        jpgheaders.cpp
        color.cpp
        dct.cpp
        quantization.cpp
        block_coding.cpp
        jpegenc.cpp
        )