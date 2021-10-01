include(CheckCXXCompilerFlag)

if(JUTTA_DRIVER_STATIC_ANALYZE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        check_cxx_compiler_flag("-fanalyzer" HAS_GCC_STATIC_ANALYZER)
        if(HAS_GCC_STATIC_ANALYZER)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fanalyzer")
            set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -fanalyzer")
            message(STATUS "GCC static analysis successfully enabled.")
        else()
            message(FATAL_ERROR "Failed to enable GCC static analysis since the compiler does not support it.")
        endif()
    else()
        message(FATAL_ERROR "Failed to enable GCC static analysis since the compiler does not seam to be GCC (GNU).")
    endif()
endif()
