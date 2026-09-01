# Application binaries use the dynamically serviced Universal CRT while
# third-party libraries remain statically linked into the AppContainer image.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# Control-flow and Spectre mitigations must be present while each dependency is
# compiled. /CETCOMPAT is an x64-only linker contract in this repository and is
# therefore intentionally absent from the ARM64 dependency triplet.
set(VCPKG_C_FLAGS "/guard:cf /Qspectre")
set(VCPKG_CXX_FLAGS "/guard:cf /Qspectre")
set(VCPKG_LINKER_FLAGS "/guard:cf")
