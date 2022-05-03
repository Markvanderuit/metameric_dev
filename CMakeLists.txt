cmake_minimum_required(VERSION 3.22)

# Specify vcpkg toolchain file
# set(VCPKG_TARGET_TRIPLET x64-mingw-static CACHE STRING "VCPKG Target Triplet to use")
set(CMAKE_TOOLCHAIN_FILE 
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake
  CACHE STRING "Vcpkg toolchain file")
  
project(Metameric LANGUAGES CXX)

# Include third party libraries provided through vcpkg
find_package(Eigen3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb.h")
find_package(TBB CONFIG REQUIRED)

# Include third party libraries provided directly
add_subdirectory(third_party/small_gl)

# Simple function to add library with basic settings
function(add_library_r target dir)
  file(GLOB_RECURSE srcs 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/metameric/${dir}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/metameric/${dir}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/*.cpp
  )
  add_library(${target} ${srcs})
  target_include_directories(${target} PUBLIC include)
  target_compile_features(${target} PRIVATE cxx_std_23)
endfunction()

function(add_executable_r target src)
  add_executable(${target} ${src})
  target_compile_features(${target} PRIVATE cxx_std_20)
endfunction()

# Set up core library
add_library_r(core core)
target_link_libraries(core PUBLIC
  Eigen3::Eigen
  fmt::fmt-header-only
)
target_include_directories(core PRIVATE ${STB_INCLUDE_DIRS})

# Set up gui library
add_library_r(gui gui)
target_link_libraries(gui PUBLIC
  core
  small_gl
  imgui::imgui
  imguizmo::imguizmo
)

# Set up main executable
add_executable_r(metameric src/metameric.cpp)
target_link_libraries(metameric PRIVATE gui)
# target_link_libraries(main_playground PRIVATE
#   core
#   small_gl
#   Eigen3::Eigen
#   fmt::fmt-header-only
#   imgui::imgui
#   imguizmo::imguizmo
#   TBB::tbb
# )

# Set up playground executable
add_executable_r(main_playground src/main_playground.cpp)
target_link_libraries(main_playground PRIVATE
  core
  small_gl
  Eigen3::Eigen
  fmt::fmt-header-only
  imgui::imgui
  imguizmo::imguizmo
  TBB::tbb
)

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

  # TODO impl. support for deep paths!
  get_filename_component(file_name ${glsl_src} NAME)
  set(spirv_bin "${PROJECT_BINARY_DIR}/resources/shaders/${file_name}.spv")
  
  add_custom_command(
    OUTPUT ${spirv_bin}
    COMMAND ${GLSL_VALIDATOR} -G ${glsl_src} -o ${spirv_bin}
    DEPENDS ${glsl_src})
  list(APPEND spirv_bins ${spirv_bin})
endforeach()
add_custom_target(shaders DEPENDS ${spirv_bins})
