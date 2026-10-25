cmake_minimum_required(VERSION 4.2)
project(Exiv2WindowsCharsetProbe LANGUAGES C CXX)
if(NOT MSVC OR NOT WIN32)
    message(FATAL_ERROR "This qualification exercises Exiv2's actual Windows conversion backend.")
endif()
cmake_path(CONVERT "${EXIV2_SOURCE_DIRECTORY}" TO_CMAKE_PATH_LIST exiv2SourceDirectory NORMALIZE)
# These supported upstream options isolate the public charset API. Neither XMP
# nor INI configuration participates in its Windows implementation. This is not
# proof of the separately configured production Adobe/XMP package integration.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_XMP OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_EXTERNAL_XMP OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_INIH OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_PNG OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_BMFF OFF CACHE BOOL "" FORCE)
set(EXIV2_ENABLE_BROTLI OFF CACHE BOOL "" FORCE)
set(EXIV2_BUILD_EXIV2_COMMAND OFF CACHE BOOL "" FORCE)
set(EXIV2_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(EXIV2_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(EXIV2_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_Iconv ON)
if(EXIV2_INSTALLED_PREFIX)
    cmake_path(CONVERT "${EXIV2_INSTALLED_PREFIX}" TO_CMAKE_PATH_LIST installedPrefix NORMALIZE)
    list(PREPEND CMAKE_PREFIX_PATH "${installedPrefix}")
    find_package(exiv2 CONFIG REQUIRED)
else()
    add_subdirectory("${exiv2SourceDirectory}" exiv2)
endif()
add_executable(Exiv2WindowsCharsetProbe CharsetCases.cpp)
target_compile_features(Exiv2WindowsCharsetProbe PRIVATE cxx_std_20)
target_compile_options(Exiv2WindowsCharsetProbe PRIVATE /W4 /WX)
target_link_libraries(Exiv2WindowsCharsetProbe PRIVATE Exiv2::exiv2lib)
# This is an uninstalled static source-tree consumer: use the real generated
# export header and its documented static-library definition.
target_include_directories(Exiv2WindowsCharsetProbe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/TestSupport")
if(NOT EXIV2_INSTALLED_PREFIX)
    target_include_directories(Exiv2WindowsCharsetProbe PRIVATE "${CMAKE_BINARY_DIR}")
    target_compile_definitions(Exiv2WindowsCharsetProbe PRIVATE exiv2lib_STATIC)
endif()
enable_testing()
add_test(NAME WindowsCharsetCompatibility COMMAND Exiv2WindowsCharsetProbe)

if(EXIV2_INSTALLED_PREFIX)
    # This route exercises the actual production feature graph and public native
    # package configuration, not the minimal source-only type qualification.
    return()
endif()
if(NOT EXISTS "${CLANG_TIDY_EXECUTABLE}")
    message(FATAL_ERROR "The selected Visual Studio distribution must supply native clang-tidy.")
endif()
# CMake owns the actual target's include/definition expansion and argv quoting;
# clang-tidy owns the C++ AST. No source-text/command-line parser is introduced.
add_custom_target(Exiv2WindowsCharsetTypes
    COMMAND "${CLANG_TIDY_EXECUTABLE}" "${exiv2SourceDirectory}/src/convert.cpp"
        --checks=-*,cppcoreguidelines-pro-type-reinterpret-cast
        --warnings-as-errors=cppcoreguidelines-pro-type-reinterpret-cast
        -- --driver-mode=cl /std:c++17 /EHsc /Dexiv2lib_STATIC
        "$<$<BOOL:$<TARGET_PROPERTY:exiv2lib,INCLUDE_DIRECTORIES>>:/I$<JOIN:$<TARGET_PROPERTY:exiv2lib,INCLUDE_DIRECTORIES>,;/I>>"
        "$<$<BOOL:$<TARGET_PROPERTY:exiv2lib,COMPILE_DEFINITIONS>>:/D$<JOIN:$<TARGET_PROPERTY:exiv2lib,COMPILE_DEFINITIONS>,;/D>>"
    COMMAND_EXPAND_LISTS VERBATIM)
