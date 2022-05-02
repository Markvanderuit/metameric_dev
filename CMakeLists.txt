cmake_minimum_required(VERSION 3.22)

# Specify vcpkg toolchain file
# set(VCPKG_TARGET_TRIPLET x64-mingw-static CACHE STRING "VCPKG Target Triplet to use")
set(CMAKE_TOOLCHAIN_FILE 
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake
  CACHE STRING "Vcpkg toolchain file")
  
project(Metameric LANGUAGES CXX)

# Include third party libraries provided through vcpkg
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

function(init_target_r target srcs)
  target_include_directories(${target} PUBLIC include)
  target_compile_features(${target} PRIVATE cxx_std_23)
  # target_compile_options(${target} PRIVATE -fsanitize=address)
endfunction()

function(add_library_r target dir)
  file(GLOB_RECURSE srcs 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/metameric/${dir}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/metameric/${dir}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/*.cpp
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

add_library_r(gl gl)
target_link_libraries(gl PRIVATE
  Eigen3::Eigen
  glad::glad
  glfw
  fmt::fmt-header-only
)

set(link_libraries
  core
  gl
  glad::glad
  Eigen3::Eigen
  fmt::fmt-header-only
  glfw
  imgui::imgui
  imguizmo::imguizmo
)

add_executable_r(main_playground src/main_playground.cpp)
target_link_libraries(main_playground PUBLIC ${link_libraries})

# add_executable_r(main src/main.cpp)
# add_executable_r(main_imgui src/main_imgui.cpp)
# add_executable_r(main_vk src/main_vk.cpp)

# target_link_libraries(main PUBLIC ${link_libraries})
# target_link_libraries(main_imgui PUBLIC ${link_libraries})
# target_link_libraries(main_vk PUBLIC ${link_libraries})

# Quickly find all shaders
file(GLOB_RECURSE glsl_srcs
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*.geom
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*.comp
)

# Quick hack to compile shaders into spir-v
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
foreach(glsl_src ${glsl_srcs})
  message(STATUS "Building shader : ${glsl_src}")

  # TODO impl. support for recursive paths!
  get_filename_component(file_name ${glsl_src} NAME)
  set(spirv_bin "${PROJECT_BINARY_DIR}/resources/shaders/${file_name}.spv")
  
  add_custom_command(
    OUTPUT ${spirv_bin}
    COMMAND ${GLSL_VALIDATOR} -G ${glsl_src} -o ${spirv_bin}
    DEPENDS ${glsl_src})
  list(APPEND spirv_bins ${spirv_bin})
endforeach()
add_custom_target(shaders DEPENDS ${spirv_bins})
