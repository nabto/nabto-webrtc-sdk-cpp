set(src
    curl_http_client/curl_async.cpp
)

add_library( integration_test_util "${src}")

find_package(CURL REQUIRED)
find_package(LibDataChannel REQUIRED)
find_package(OpenSSL REQUIRED)


target_link_libraries(integration_test_util
    LibDataChannel::LibDataChannel
    CURL::libcurl
    OpenSSL::Crypto
    nabto_webrtc_signaling_device
)

target_include_directories(integration_test_util
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_sources(integration_test_util PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS .
    FILES
        curl_http_client/curl_async.hpp
        libdatachannel_websocket/rtc_websocket_wrapper.hpp
        timer/timer.hpp
)
