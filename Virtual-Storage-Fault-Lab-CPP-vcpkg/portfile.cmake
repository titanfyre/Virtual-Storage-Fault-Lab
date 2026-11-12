vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/titanfyre/Virtual-Storage-Fault-Lab/releases/download/sdk-v0.1.1/virtual-storage-fault-lab-sdk-0.1.1.tar.gz"
    FILENAME "virtual-storage-fault-lab-sdk-0.1.1.tar.gz"
    SHA512 09b49c5c7e72bef8657ef12839114701dee61d9e74bcf746e9762f0d34cfa7bf5e432afe1128601e075a601df47927d3edaec3fa073e4d15a3d7e2171e9c1bcd
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}" NO_REMOVE_ONE_LEVEL)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME VirtualStorageFaultLabSDK CONFIG_PATH lib/cmake/VirtualStorageFaultLabSDK)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
