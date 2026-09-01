# Application binaries use the dynamically serviced Universal CRT while
# third-party libraries remain statically linked into the AppContainer image.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# Control-flow and Spectre mitigations must be present while each dependency is
# compiled. CET compatibility belongs only to the final x64 application link,
# so it is intentionally absent from every dependency triplet.
set(VCPKG_C_FLAGS "/guard:cf /Qspectre")
set(VCPKG_CXX_FLAGS "/guard:cf /Qspectre")
set(VCPKG_LINKER_FLAGS "/guard:cf")
