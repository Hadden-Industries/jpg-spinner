# This closed repository port uses Exiv2's public external-XMP capability.
# The only parser-source change selects the selected SDK's native MD5 signature.
vcpkg_check_linkage(ONLY_STATIC_LIBRARY ONLY_DYNAMIC_CRT)
if(NOT "xmp" IN_LIST FEATURES)
    message(FATAL_ERROR "The reviewed metadata build requires the xmp feature.")
endif()
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Exiv2/exiv2
    REF "v${VERSION}"
    SHA512 a7fa8fb19e54cdf0b9aac917087c86d45ea1b4a991a85ea8c5dd96af0289df63823901e44604d4c8df55883dcd19aee2efc48d7b5fde313dc0787b842a223a4a
    PATCHES
        use-native-inih-package.patch
        adobe-md5-buffer.patch
)
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DEXIV2_ENABLE_XMP=OFF
        -DEXIV2_ENABLE_EXTERNAL_XMP=ON
        -DEXIV2_ENABLE_PNG=OFF
        -DEXIV2_ENABLE_BMFF=OFF
        -DEXIV2_ENABLE_BROTLI=OFF
        -DEXIV2_ENABLE_NLS=OFF
        -DEXIV2_ENABLE_VIDEO=OFF
        -DEXIV2_ENABLE_WEBREADY=OFF
        -DEXIV2_ENABLE_CURL=OFF
        -DEXIV2_ENABLE_DYNAMIC_RUNTIME=ON
        -DEXIV2_BUILD_EXIV2_COMMAND=OFF
        -DEXIV2_BUILD_UNIT_TESTS=OFF
        -DEXIV2_BUILD_SAMPLES=OFF
        -DEXIV2_BUILD_DOC=OFF
        -DCMAKE_DISABLE_FIND_PACKAGE_Python3=ON
)
vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/exiv2)
vcpkg_fixup_pkgconfig()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share" "${CURRENT_PACKAGES_DIR}/share/man")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
