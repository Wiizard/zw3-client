cmake_minimum_required(VERSION 3.24)
project(zw3_zlib LANGUAGES C)

include(FetchContent)
set(ZLIB_COMPAT ON CACHE BOOL "" FORCE)
set(ZLIB_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(ZLIBNG_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
FetchContent_Declare(zlib_ng
    URL https://github.com/zlib-ng/zlib-ng/archive/425439062b114a0f6cf625022c41d929c7e879f9.tar.gz
    URL_HASH SHA256=ba8f446a0bb26a194d7619fc0c1ffe58ac5a86dc2d6b61e24e9e09e3f56de4c1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(zlib_ng)
set_target_properties(zlib PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
