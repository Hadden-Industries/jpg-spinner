cmake_minimum_required(VERSION 3.25)
project(AdobeXmpCorePackage LANGUAGES C CXX)
find_package(expat CONFIG REQUIRED)

# Upstream's Windows build sets its own common flags. Preserve the toolchain's
# mitigation and sanitizer flags through its supported extra-flags inputs.
set(COMMON_EXTRA_C_COMPILE_FLAGS "${CMAKE_C_FLAGS}")
set(COMMON_EXTRA_CXX_COMPILE_FLAGS "${CMAKE_CXX_FLAGS}")
set(XMP_BUILD_STATIC ON)
if(ADOBE_XMP_ARCHITECTURE STREQUAL "x86")
    set(CMAKE_CL_64 OFF)
else()
    set(CMAKE_CL_64 ON)
endif()
string(TOUPPER "${ADOBE_XMP_ARCHITECTURE}" CMAKE_ARCH)
add_subdirectory("${ADOBE_XMP_SOURCE}/XMPCore/build" sdk)
target_link_libraries(XMPCoreStatic PRIVATE expat::expat)
# The checked conversion repair uses C++20 bit_cast for the SDK's documented
# hexadecimal signed bit patterns, avoiding aliasing or implementation-defined
# signed narrowing. This requirement is private to the native implementation.
target_compile_features(XMPCoreStatic PRIVATE cxx_std_20)
# CMake derives the archive machine type from the selected compiler; do not
# retain the SDK's hard-coded /machine:x64 when building ARM64.
set_target_properties(XMPCoreStatic PROPERTIES
    # Static package consumers must retain the full compiler debug information
    # without a private build-tree PDB. CMP0141 is NEW from our top-level 3.25
    # minimum; CMake owns the corresponding MSVC /Z7 option for every config.
    # https://cmake.org/cmake/help/latest/policy/CMP0141.html
    # https://learn.microsoft.com/cpp/build/reference/z7-zi-zi-debug-information-format
    MSVC_DEBUG_INFORMATION_FORMAT Embedded
    STATIC_LIBRARY_FLAGS ""
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
    EXPORT_NAME Core)
target_compile_definitions(XMPCoreStatic INTERFACE WIN_ENV=1 XMP_StaticBuild=1)
target_include_directories(XMPCoreStatic INTERFACE
    "$<INSTALL_INTERFACE:include/adobe-xmp/public/include>"
    "$<INSTALL_INTERFACE:include/adobe-xmp/third-party/zuid/interfaces>"
    "$<INSTALL_INTERFACE:include/adobe-xmp>")
install(TARGETS XMPCoreStatic EXPORT AdobeXmpTargets ARCHIVE DESTINATION lib)
install(EXPORT AdobeXmpTargets NAMESPACE AdobeXmp:: DESTINATION lib/cmake/XmpSdk)
install(DIRECTORY "${ADOBE_XMP_SOURCE}/public/include/" DESTINATION include/adobe-xmp/public/include)
install(FILES "${ADOBE_XMP_SOURCE}/third-party/zuid/interfaces/MD5.h"
    DESTINATION include/adobe-xmp/third-party/zuid/interfaces)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/XmpSdkConfig.cmake" DESTINATION lib/cmake/XmpSdk)
