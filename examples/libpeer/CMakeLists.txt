cmake_minimum_required(VERSION 3.20)

project(NabtoSignaling_libpeer_example
    VERSION 0.0.1
    LANGUAGES CXX)

include(FetchContent)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)

FetchContent_Declare(NabtoWebrtcSignaling OVERRIDE_FIND_PACKAGE SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../sdk")
FetchContent_Declare(peer OVERRIDE_FIND_PACKAGE SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../3rdparty/libpeer")

FetchContent_MakeAvailable(NabtoWebrtcSignaling)
FetchContent_MakeAvailable(peer)

find_package(plog REQUIRED)
find_package(libwebsockets REQUIRED)
find_package(NabtoWebrtcSignaling REQUIRED)
find_package(peer REQUIRED)


set(src
    src/main.cpp
    src/websocket_wrapper.cpp
    src/webrtc_connection.cpp
    src/lws_context_manager.cpp
    src/http_client.cpp
)

add_executable(libpeer_device "${src}")
target_link_libraries(libpeer_device
    plog::plog
    NabtoWebrtcSignaling::device
    NabtoWebrtcSignaling::util_logging
    NabtoWebrtcSignaling::util_std_timer
    NabtoWebrtcSignaling::util_uuid
    NabtoWebrtcSignaling::util_message_transport
    NabtoWebrtcSignaling::util_token_generator

    NabtoWebrtcSignaling::util_curl_client
    
    websockets
    peer
)

install(TARGETS libpeer_device RUNTIME DESTINATION bin)
