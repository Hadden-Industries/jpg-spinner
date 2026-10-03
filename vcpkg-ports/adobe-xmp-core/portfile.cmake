# Repository-owned packaging for the approved native preservation repair.
# Source identity and patch bytes participate in vcpkg's binary-cache ABI.
vcpkg_check_linkage(ONLY_STATIC_LIBRARY ONLY_DYNAMIC_CRT)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO adobe/XMP-Toolkit-SDK
    REF 581c41213ddcee1fbc72cbb532531102a6617a25
    SHA512 a2fe6a9f14569398b1aed7a36c950b73d7a0bcb785269125280f0af4bd0157c9cc3de4656935b82c9e37fb5ec2ca50891f3f506f187388f556fb3fe319311116
    PATCHES
        preserve-localized-text.patch
        use-packaged-expat.patch
)

# Keep upstream's source selection and compiler configuration. The package
# entry point only supplies dependency linkage and relocatable installation.
vcpkg_cmake_configure(
    SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}"
    OPTIONS
        "-DADOBE_XMP_SOURCE=${SOURCE_PATH}"
        "-DADOBE_XMP_ARCHITECTURE=${VCPKG_TARGET_ARCHITECTURE}"
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME XmpSdk CONFIG_PATH lib/cmake/XmpSdk)
vcpkg_copy_pdbs()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
# Includes the RSA Data Security, Inc. MD5 Message-Digest Algorithm notice.
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE" "${SOURCE_PATH}/third-party/zuid/interfaces/MD5.h")
