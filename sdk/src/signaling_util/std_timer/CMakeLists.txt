add_library( nabto_webrtc_std_timer INTERFACE)

set_target_properties(nabto_webrtc_std_timer PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(nabto_webrtc_std_timer
)

target_include_directories(nabto_webrtc_std_timer
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_sources(nabto_webrtc_std_timer PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ./include
    FILES
        include/nabto/webrtc/util/std_timer.hpp
)

