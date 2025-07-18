include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_scripts/nabto_version.cmake")
cmake_policy(SET CMP0007 NEW)

if ("${NABTO_WEBRTC_VERSION}" STREQUAL "")

  nabto_version(version_out version_error)

  if (NOT version_out)
    if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/api/version.cpp)

      message(FATAL_ERROR "No file ${CMAKE_CURRENT_SOURCE_DIR}/api/version.cpp exists and it cannot be auto generated. ${version_error}")
    endif()
  endif()
else()
  set(version_out ${NABTO_WEBRTC_VERSION})
endif()

if (version_out)
  set(VERSION "#include <nabto/webrtc/device.hpp>\n
#include <string>\n
#include <array>\n
constexpr std::array<char, 100> version_str {\"${version_out}\"};
std::string nabto::webrtc::SignalingDevice::version() { return {std::begin(version_str),std::end(version_str)}; }\n")

  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp VERSION_)
  else()
    set(VERSION_ "")
  endif()

  if (NOT "${VERSION}" STREQUAL "${VERSION_}")
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp "${VERSION}")
  endif()
endif()
