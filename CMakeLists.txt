cmake_minimum_required(VERSION 3.22)

option(ENABLE_CUDA "Enable building of CUDA backend if ON" OFF)

# Specify vcpkg toolchain file
set(CMAKE_TOOLCHAIN_FILE 
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake
  CACHE STRING "Vcpkg toolchain file")
  
if(ENABLE_CUDA)
  project(Metameric LANGUAGES CXX CUDA)
else()
  project(Metameric LANGUAGES CXX)
endif()

# Include third party libraries provided through vcpkg
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

# Include remaining third party libraries
find_package(Thrust CONFIG REQUIRED)
thrust_create_target(Thrust)

function(init_target_r target srcs)
  target_include_directories(${target} PUBLIC include)
  if(ENABLE_CUDA)
    set_source_files_properties(srcs PROPERTIES LANGUAGE CUDA)
    target_compile_features(${target} PRIVATE cuda_std_17)
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:
      --use_fast_math
      --expt-relaxed-constexpr
      --extended-lambda
      --forward-unknown-to-host-compiler
    >)
  else()
    target_compile_features(${target} PRIVATE cxx_std_20)
  endif()
  # target_compile_options(${target} PRIVATE -fsanitize=address)
endfunction()

function(add_library_r target dir)
  file(GLOB_RECURSE srcs 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/metameric/${dir}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/metameric/${dir}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/*.cu
  )
  add_library(${target} ${srcs})
  init_target_r(${target} ${srcs})
endfunction()

function(add_executable_r target src)
  add_executable(${target} ${src})
  init_target_r(${target} ${src}})
endfunction()

add_library_r(core core)
target_link_libraries(core PRIVATE
  glad::glad
  Eigen3::Eigen
  glfw
  fmt::fmt-header-only
)

set(link_libraries
  core
  glad::glad
  Eigen3::Eigen
  fmt::fmt-header-only
  glfw
  imgui::imgui
  imguizmo::imguizmo
  Thrust
)

add_executable_r(main src/main.cpp)
add_executable_r(main_imgui src/main_imgui.cpp)
add_executable_r(main_playground src/main_playground.cpp)
target_link_libraries(main PUBLIC ${link_libraries})
target_link_libraries(main_imgui PUBLIC ${link_libraries})
target_link_libraries(main_playground PUBLIC ${link_libraries})