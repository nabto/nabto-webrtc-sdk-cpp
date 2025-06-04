set(src
    main.cpp
    ../utils/rtp-client/rtp_client.cpp
)

add_executable(webrtc_device "${src}")

find_package(plog REQUIRED)
find_package(cxxopts)
find_package(LibDataChannel REQUIRED)
find_package(CURL REQUIRED)
find_package(NabtoWebrtcSignaling REQUIRED)
find_package(OpenSSL REQUIRED)

target_link_libraries(webrtc_device
    cxxopts::cxxopts
    LibDataChannel::LibDataChannel
    CURL::libcurl
    plog::plog
    NabtoWebrtcSignaling::device
    NabtoWebrtcSignaling::util_logging
    NabtoWebrtcSignaling::util_curl_client
    # NabtoWebrtcSignaling::util_std_timer
    OpenSSL::Crypto
    webrtc_example_common
)

install(TARGETS webrtc_device
    RUNTIME DESTINATION bin
)
