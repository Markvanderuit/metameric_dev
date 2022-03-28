cmake_minimum_required(VERSION 3.22)

# Specify vcpkg toolchain file
set(CMAKE_TOOLCHAIN_FILE 
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake
  CACHE STRING "Vcpkg toolchain file")

project(SpectralUplifting LANGUAGES CXX CUDA)

# Include third party libraries provided through vcpkg
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_executable(main src/main.cpp)
target_link_libraries(main PUBLIC
  glad::glad
  glfw
  imgui::imgui
)