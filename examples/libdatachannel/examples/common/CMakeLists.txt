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
)
