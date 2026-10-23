cmake_minimum_required(VERSION 3.25)
project(AdobeXmpDebugInformationProbe LANGUAGES CXX)

# Consume the installed public package, not objects from its build directory.
# Missing compiler PDBs must fail here even when a developer's source/build
# cache has disappeared. Microsoft's LINK /WX turns LNK4099 into a real failure.
find_package(XmpSdk CONFIG REQUIRED)
# The production export gives Exiv2 priority over its bundled SDK headers.
# This standalone public client has no competing SDK; retain CMake's normal
# imported-header classification, matching the repository's external headers.
# LINK /WX below remains unconditional for the installed archive's diagnostics.
set_property(TARGET AdobeXmp::Core PROPERTY IMPORTED_NO_SYSTEM FALSE)
add_executable(AdobeXmpDebugInformationProbe AdobeXmpDebugInformationProbe.cpp)
target_compile_features(AdobeXmpDebugInformationProbe PRIVATE cxx_std_20)
target_link_libraries(AdobeXmpDebugInformationProbe PRIVATE AdobeXmp::Core)
target_compile_options(AdobeXmpDebugInformationProbe PRIVATE /W4 /WX /utf-8)
target_link_options(AdobeXmpDebugInformationProbe PRIVATE /DEBUG /WX /INCREMENTAL:NO)

enable_testing()
add_test(NAME PublicAdobeXmpInitialization COMMAND AdobeXmpDebugInformationProbe)
