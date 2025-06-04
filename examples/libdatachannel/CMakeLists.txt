
cmake_minimum_required(VERSION 3.20)

project(NabtoSignaling_libdatachannel_example
    VERSION 0.0.1
    LANGUAGES CXX)


include(FetchContent)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

FetchContent_Declare(NabtoWebrtcSignaling OVERRIDE_FIND_PACKAGE SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../sdk")
FetchContent_MakeAvailable(NabtoWebrtcSignaling)

add_subdirectory(examples/common)
add_subdirectory(examples/webrtc_device)
add_subdirectory(examples/webrtc_device_rtsp)
