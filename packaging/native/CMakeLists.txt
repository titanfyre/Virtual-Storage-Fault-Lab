cmake_minimum_required(VERSION 3.24)
project(VirtualStorageFaultLabSDK VERSION 0.1.1 LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This SDK requires Windows x64. No driver is installed by this build.")
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
add_library(TitanVirtualStorageFaultLab STATIC
  "sdk/SDK/StorageFault.cpp"
)
add_library(VirtualStorageFaultLabSDK::sdk ALIAS TitanVirtualStorageFaultLab)
set_target_properties(TitanVirtualStorageFaultLab PROPERTIES EXPORT_NAME sdk)
target_compile_features(TitanVirtualStorageFaultLab PUBLIC cxx_std_17)
target_compile_definitions(TitanVirtualStorageFaultLab PUBLIC )
target_compile_definitions(TitanVirtualStorageFaultLab PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
if(MSVC)
  target_compile_options(TitanVirtualStorageFaultLab PRIVATE /utf-8 /EHsc /W4)
endif()
target_include_directories(TitanVirtualStorageFaultLab PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk>"
  "$<INSTALL_INTERFACE:include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk/SDK>"
  "$<INSTALL_INTERFACE:include/SDK>"
)
target_link_libraries(TitanVirtualStorageFaultLab PUBLIC advapi32 comctl32 crypt32 gdi32 newdev ole32 setupapi shell32 user32 windowscodecs wintrust)
install(TARGETS TitanVirtualStorageFaultLab EXPORT VirtualStorageFaultLabSDKTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY sdk/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl")
install(FILES LICENSE DESTINATION share/virtual-storage-fault-lab)
install(EXPORT VirtualStorageFaultLabSDKTargets NAMESPACE VirtualStorageFaultLabSDK:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VirtualStorageFaultLabSDK)
configure_file("VirtualStorageFaultLabSDKConfig.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/VirtualStorageFaultLabSDKConfig.cmake" @ONLY)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/VirtualStorageFaultLabSDKConfigVersion.cmake" VERSION 0.1.1 COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/VirtualStorageFaultLabSDKConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/VirtualStorageFaultLabSDKConfigVersion.cmake" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VirtualStorageFaultLabSDK)
