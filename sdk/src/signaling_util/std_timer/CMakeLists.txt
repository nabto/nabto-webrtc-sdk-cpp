set(src
)

add_library( nabto_webrtc_std_timer "${src}")

set_target_properties(nabto_webrtc_std_timer PROPERTIES LINKER_LANGUAGE CXX)

find_package(plog REQUIRED)

target_link_libraries(nabto_webrtc_std_timer
    plog::plog
)

target_include_directories(nabto_webrtc_std_timer
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_sources(nabto_webrtc_std_timer PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ..
    FILES
        std_timer.hpp
)

