set(src
    webrtc_connection/webrtc_connection.cpp
)

add_library( webrtc_example_common "${src}")

find_package(CURL REQUIRED)
find_package(LibDataChannel REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(NabtoWebrtcSignaling REQUIRED)


target_link_libraries(webrtc_example_common
    LibDataChannel::LibDataChannel
    CURL::libcurl
    OpenSSL::Crypto
    NabtoWebrtcSignaling::util_logging
    NabtoWebrtcSignaling::util_curl_client
    NabtoWebrtcSignaling::util_std_timer
    NabtoWebrtcSignaling::util_uuid
    NabtoWebrtcSignaling::device
)

target_include_directories(webrtc_example_common
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_sources(webrtc_example_common PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS .
    FILES
        libdatachannel_websocket/rtc_websocket_wrapper.hpp
        message_signer/message_signer.hpp
        message_signer/none_message_signer.hpp
        message_signer/shared_secret_message_signer.hpp
        token_generator/nabto_jwt.hpp
        token_generator/openssl_key_id.hpp
)


if(NOT CMAKE_CROSSCOMPILING)
    message(STATUS "Configuring tests: Native build detected.")

    find_package(GTest CONFIG REQUIRED)
    enable_testing()

    add_executable(
        webrtc_device_test
        ../common/token_generator/openssl_key_id_test.cpp
    )

    target_link_libraries(
        webrtc_device_test
        PRIVATE
        GTest::gtest
        GTest::gtest_main
        OpenSSL::Crypto
    )

    add_test(NAME webrtc_device_test COMMAND webrtc_device_test)

    include(GoogleTest)
    gtest_discover_tests(webrtc_device_test)
else()
    message(STATUS "Skipping tests: Cross-compilation detected.")
endif()
