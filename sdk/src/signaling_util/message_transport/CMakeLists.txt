set(curl_src
    src/message_transport.cpp
    src/message_transport_impl.cpp
    src/shared_secret_message_signer_impl.cpp
)

find_package(jwt-cpp CONFIG REQUIRED)

add_library( nabto_webrtc_message_transport "${curl_src}")

target_link_libraries(nabto_webrtc_message_transport
    NabtoWebrtcSignaling::util_uuid
    NabtoWebrtcSignaling::util_logging
    NabtoWebrtcSignaling::device
    jwt-cpp::jwt-cpp
)

target_compile_definitions(nabto_webrtc_message_transport PRIVATE -DJWT_DISABLE_PICOJSON)

target_include_directories(nabto_webrtc_message_transport
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_sources(nabto_webrtc_message_transport PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ./include
    FILES
        include/nabto/webrtc/util/message_transport.hpp
)
