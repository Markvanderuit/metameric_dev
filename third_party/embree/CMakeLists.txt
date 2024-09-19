cmake_minimum_required(VERSION 3.22)

if (WIN32)
  list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/windows")
elseif(UNIX)
  list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/linux")
else()
  message(FATAL_ERROR "No Intel Embree package included for this platform")
endif()