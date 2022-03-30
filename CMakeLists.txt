cmake_minimum_required(VERSION 3.22)

# Specify vcpkg toolchain file
set(CMAKE_TOOLCHAIN_FILE 
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake
  CACHE STRING "Vcpkg toolchain file")

project(Metameric LANGUAGES CXX CUDA)

# Include third party libraries provided through vcpkg
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

# Define function to add a library while recursively globbing its files from a specified directory
function(add_library_r target dir)
  file(GLOB_RECURSE srcs 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/metameric/${dir}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/metameric/${dir}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/*.cu
  )
  add_library(${target} ${srcs})
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_include_directories(${target} PUBLIC include)
endfunction()

add_library_r(core core)
# add_library_r(gui gui)

add_executable(main src/main.cpp)

target_link_libraries(main PUBLIC core glad::glad glfw imgui::imgui)
# target_link_libraries(gui PUBLIC core)
# target_link_libraries(main PUBLIC core gui)