# Isolated release-only sanitizer dependencies. Mixing annotated consumer STL
# objects with the ordinary libraries violates MSVC's container-annotation ABI.
# Keep all normal mitigation flags; do not suppress annotation mismatch checks.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_C_FLAGS "/guard:cf /Qspectre /fsanitize=address /Zi")
set(VCPKG_CXX_FLAGS "/guard:cf /Qspectre /fsanitize=address /Zi")
set(VCPKG_LINKER_FLAGS "/guard:cf /DEBUG /INCREMENTAL:NO")
