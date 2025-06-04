
add_library( nabto_webrtc_logging INTERFACE)

set_target_properties(nabto_webrtc_logging PROPERTIES LINKER_LANGUAGE CXX)

find_package(plog REQUIRED)

target_link_libraries(nabto_webrtc_logging INTERFACE
    plog::plog
)

target_include_directories(nabto_webrtc_logging
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_sources(nabto_webrtc_logging PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ./include
    FILES
        include/nabto/webrtc/util/logging.hpp
)

