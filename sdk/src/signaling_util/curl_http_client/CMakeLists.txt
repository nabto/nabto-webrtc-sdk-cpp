set(curl_src
    src/curl_async.cpp
)

add_library( nabto_webrtc_curl_client "${curl_src}")

find_package(CURL REQUIRED)
find_package(NabtoWebrtcSignaling REQUIRED)

target_link_libraries(nabto_webrtc_curl_client
    CURL::libcurl
    NabtoWebrtcSignaling::util_logging
    NabtoWebrtcSignaling::device
)

target_include_directories(nabto_webrtc_curl_client
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_sources(nabto_webrtc_curl_client PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ./include
    FILES
        include/nabto/webrtc/util/curl_async.hpp
)

