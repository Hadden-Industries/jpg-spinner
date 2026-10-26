cmake_minimum_required(VERSION 4.2)
project(CodecBoundaryProbe LANGUAGES CXX)

find_package(JPEG REQUIRED)
find_package(SPNG CONFIG REQUIRED)
add_executable(CodecBoundaryProbe
    BoundaryCases.cpp
    ../../tests/TestSupport/DeterministicJpegFixtureFactory.cpp)
target_compile_features(CodecBoundaryProbe PRIVATE cxx_std_20)
target_include_directories(CodecBoundaryProbe PRIVATE ../../tests/TestSupport)
target_link_libraries(CodecBoundaryProbe PRIVATE JPEG::JPEG spng::spng_static)
