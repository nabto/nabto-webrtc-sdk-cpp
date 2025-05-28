cmake_minimum_required(VERSION 3.30)
project(NabtoSignaling
    VERSION 0.0.1
    LANGUAGES CXX
)


option(NABTO_SIGNALING_BUILD_TESTS "Build tests" OFF)

set(src
    src/signaling_device_impl.cpp
    src/signaling_channel_impl.cpp
    src/signaling_device_factory.cpp
    src/websocket_connection.cpp
    src/signaling_error.cpp
    src/signaling.cpp
)

add_library(nabto_signaling_core ${src})

target_include_directories(nabto_signaling_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(nlohmann_json REQUIRED)
find_package(plog REQUIRED)

target_link_libraries(nabto_signaling_core nlohmann_json::nlohmann_json plog::plog)

if (NABTO_SIGNALING_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    enable_testing()
    add_executable(
        nabto_signaling_test
        test/create_destroy_signaling_device.cpp
        src/signaling_device_impl_test.cpp
    )
    target_link_libraries(
        nabto_signaling_test
        nabto_signaling_core
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
        nabto_signaling_core
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

add_library("${PROJECT_NAME}::core" ALIAS nabto_signaling_core)

install(
    TARGETS nabto_signaling_core
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
