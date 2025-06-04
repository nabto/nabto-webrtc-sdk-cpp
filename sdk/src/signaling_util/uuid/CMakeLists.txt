
add_library( nabto_webrtc_uuid INTERFACE)

set_target_properties(nabto_webrtc_uuid PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(nabto_webrtc_uuid INTERFACE
)

target_include_directories(nabto_webrtc_uuid
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_sources(nabto_webrtc_uuid PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ..
    FILES
        uuid.hpp
)

