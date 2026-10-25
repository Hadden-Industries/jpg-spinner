# Exercise the dependency's real configure module, rather than another copy of
# its C++ snippet. The module itself owns native compile/link/run decisions.
cmake_minimum_required(VERSION 4.2)
project(Exiv2FilesystemProbe LANGUAGES CXX)
if(NOT MSVC OR NOT IS_DIRECTORY "${EXIV2_SOURCE_DIRECTORY}/cmake")
    message(FATAL_ERROR "This probe requires MSVC and a prepared Exiv2 source directory.")
endif()
cmake_path(CONVERT "${EXIV2_SOURCE_DIRECTORY}" TO_CMAKE_PATH_LIST exiv2SourceDirectory NORMALIZE)
list(PREPEND CMAKE_MODULE_PATH "${exiv2SourceDirectory}/cmake")
# Windows path::value_type is wchar_t. Make the actual %s/wide-pointer defect a
# deterministic compiler failure while leaving all other diagnostics visible.
set(CMAKE_REQUIRED_FLAGS "/we4477")
find_package(Filesystem REQUIRED MODULE)
if(NOT TARGET std::filesystem OR NOT CXX_FILESYSTEM_NO_LINK_NEEDED)
    message(FATAL_ERROR "The native filesystem probe failed on the selected Windows toolchain.")
endif()
