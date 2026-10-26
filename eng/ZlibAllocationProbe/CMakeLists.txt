cmake_minimum_required(VERSION 4.2)
project(ZlibAllocationProbe LANGUAGES C)

if(NOT MSVC OR NOT IS_DIRECTORY "${ZLIB_SOURCE_DIRECTORY}")
    message(FATAL_ERROR "This probe requires the selected Windows CRT and prepared zlib declarations.")
endif()
find_package(ZLIB CONFIG REQUIRED COMPONENTS static)
add_executable(ZlibAllocationProbe AllocationCases.c)
target_compile_features(ZlibAllocationProbe PRIVATE c_std_11)
# Read the upstream private allocator declaration; link its implementation from
# the selected installed archive instead of copying or emulating that code.
target_include_directories(ZlibAllocationProbe PRIVATE "${ZLIB_SOURCE_DIRECTORY}")
target_link_libraries(ZlibAllocationProbe PRIVATE ZLIB::ZLIBSTATIC)
