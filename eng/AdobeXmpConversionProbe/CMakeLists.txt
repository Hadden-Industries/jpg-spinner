cmake_minimum_required(VERSION 4.2)
project(AdobeXmpConversionProbe LANGUAGES CXX)
if(NOT MSVC OR NOT IS_DIRECTORY "${ADOBE_XMP_SOURCE_DIRECTORY}/source")
    message(FATAL_ERROR "This probe requires the qualified Windows SDK and its prepared upstream source.")
endif()
find_package(XmpSdk CONFIG REQUIRED)
add_executable(AdobeXmpConversionProbe ConversionCases.cpp)
target_compile_features(AdobeXmpConversionProbe PRIVATE cxx_std_20)
target_include_directories(AdobeXmpConversionProbe PRIVATE "${ADOBE_XMP_SOURCE_DIRECTORY}")
target_link_libraries(AdobeXmpConversionProbe PRIVATE AdobeXmp::Core)
# Link the actual selected native SDK. Its internal conversion declarations are
# used only to qualify byte order and buffer bounds, never copied into the app.
