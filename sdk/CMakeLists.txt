cmake_minimum_required(VERSION 3.30)
project(NabtoWebrtcSignaling
    VERSION 0.0.1
    LANGUAGES CXX
)

add_subdirectory(src/signaling-device)
add_subdirectory(src/signaling-util/logging)
add_subdirectory(src/signaling-util/curl_http_client)

option(NABTO_SIGNALING_BUILD_TESTS "Build tests" OFF)

if (NABTO_SIGNALING_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    enable_testing()
    add_executable(
        nabto_signaling_test
        test/create_destroy_signaling_device.cpp
        src/signaling-device/signaling_device_impl_test.cpp
    )
    target_link_libraries(
        nabto_signaling_test
        nabto_webrtc_signaling_device
        GTest::gtest_main
    )
    include(GoogleTest)
    gtest_discover_tests(nabto_signaling_test)

    add_subdirectory(integration-test/generated/cpp)
    add_subdirectory(integration-test/util)

    add_executable(
        nabto_integration_test
        integration-test/integration_test.cpp
    )
    target_link_libraries(
        nabto_integration_test
        CppRestOpenAPIClient
        integration_test_util
        nabto_webrtc_signaling_device
        GTest::gtest_main
    )

endif()


include(GNUInstallDirs)
set(config_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(version_config "${generated_dir}/${PROJECT_NAME}ConfigVersion.cmake")
set(project_config "${generated_dir}/${PROJECT_NAME}Config.cmake")
set(TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets")
set(namespace "${PROJECT_NAME}::")

include(CMakePackageConfigHelpers)

add_library("${PROJECT_NAME}::device" ALIAS nabto_webrtc_signaling_device)

add_library("${PROJECT_NAME}::utilLogging" ALIAS nabto_webrtc_logging)

add_library("${PROJECT_NAME}::utilCurlClient" ALIAS nabto_webrtc_curl_client)

install(
    TARGETS nabto_webrtc_signaling_device nabto_webrtc_logging nabto_webrtc_curl_client
    EXPORT "${TARGETS_EXPORT_NAME}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILE_SET public_headers
)

write_basic_package_version_file(
    "${version_config}" COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "cmake-scripts/Config.cmake.in"
    "${project_config}"
    INSTALL_DESTINATION "${config_install_dir}"
)

# Config
#   * <prefix>/lib/cmake/Foo/FooConfig.cmake
#   * <prefix>/lib/cmake/Foo/FooConfigVersion.cmake
install(
    FILES "${project_config}" "${version_config}"
    DESTINATION "${config_install_dir}"
)

# Config
#   * <prefix>/lib/cmake/Foo/FooTargets.cmake
install(
    EXPORT "${TARGETS_EXPORT_NAME}"
    NAMESPACE "${namespace}"
    DESTINATION "${config_install_dir}"
)
