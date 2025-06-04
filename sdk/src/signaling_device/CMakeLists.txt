
set(src
    src/signaling_device_impl.cpp
    src/signaling_channel_impl.cpp
    src/signaling_device_factory.cpp
    src/websocket_connection.cpp
    src/signaling_error.cpp
    src/signaling.cpp
)

add_library(nabto_webrtc_signaling_device ${src})

target_include_directories(nabto_webrtc_signaling_device PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(nlohmann_json REQUIRED)
find_package(plog REQUIRED)

target_link_libraries(nabto_webrtc_signaling_device nlohmann_json::nlohmann_json plog::plog)
