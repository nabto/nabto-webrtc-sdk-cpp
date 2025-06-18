set(src
    main.cpp
    rtsp-client/rtsp_client.cpp
    rtsp-client/tcp_rtp_client.cpp
)

option(RTSP_HAS_BASIC_AUTH "disable RTSP client basic auth" ON)
option(RTSP_HAS_DIGEST_AUTH "disable RTSP client digest auth" ON)

if (RTSP_HAS_BASIC_AUTH)
    add_definitions(-DNABTO_RTSP_HAS_BASIC_AUTH)
endif()
if (RTSP_HAS_DIGEST_AUTH)
    add_definitions(-DNABTO_RTSP_HAS_DIGEST_AUTH)
endif()

add_executable(webrtc_device_rtsp "${src}")

find_package(plog REQUIRED)
find_package(cxxopts)
find_package(LibDataChannel REQUIRED)
find_package(CURL REQUIRED)

find_package(NabtoWebrtcSignaling REQUIRED)
find_package(OpenSSL REQUIRED)

target_link_libraries(webrtc_device_rtsp
    webrtc_example_common
    cxxopts::cxxopts
    LibDataChannel::LibDataChannel
    CURL::libcurl
    plog::plog
    NabtoWebrtcSignaling::device
    NabtoWebrtcSignaling::util_logging
    NabtoWebrtcSignaling::util_curl_client
    NabtoWebrtcSignaling::util_token_generator
    OpenSSL::Crypto
)

install(TARGETS webrtc_device_rtsp
    RUNTIME DESTINATION bin
)
