# Exiv2's documented external-XMP package contract, backed by a real exported
# target. Resolve the same native Expat target used to compile the SDK.
include(CMakeFindDependencyMacro)
find_dependency(expat CONFIG)
if(NOT TARGET EXPAT::EXPAT)
    add_library(EXPAT::EXPAT ALIAS expat::expat)
endif()
include("${CMAKE_CURRENT_LIST_DIR}/AdobeXmpTargets.cmake")
# Prefer this SDK's matching headers and template definitions over Exiv2's
# bundled SYSTEM include directory. Never mix the two SDK revisions.
set_target_properties(AdobeXmp::Core PROPERTIES IMPORTED_NO_SYSTEM TRUE)
get_target_property(XMPSDK_INCLUDE_DIR AdobeXmp::Core INTERFACE_INCLUDE_DIRECTORIES)
set(XMPSDK_LIBRARY AdobeXmp::Core)
set(XmpSdk_FOUND TRUE)
