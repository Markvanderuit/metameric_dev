cmake_minimum_required(VERSION 3.22)

# Specify VCPKG toolchain path, CMAKE module path
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/resources/cmake" CACHE STRING "CMake module path")

# Specify project name, languages
project(Metameric LANGUAGES CXX)

# Build options; see resources/cmake/conf.cmake for the editor's spectral layout
option(MET_ENABLE_EXCEPTIONS "Enable exceptions on release"  ON)
option(MET_ENABLE_TRACY      "Enable Tracy support"         OFF)

# Enable all modules in resources/cmake
include(metameric_conf)
include(dependency)
include(spirv)

# Specify output directories as /bin, /lib, /out, and tell MSVC to stuff it
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
if(MSVC)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG          ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG          ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL     ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE        ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG          ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL     ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
endif()

# Include the packages in /third_party
option(gl_enable_exceptions  "" ${MET_ENABLE_EXCEPTIONS})
option(gl_enable_tracy       "" ${MET_ENABLE_TRACY})
add_subdirectory(third_party/autodiff)
add_subdirectory(third_party/rapidobj)
add_subdirectory(third_party/small_gl)
add_subdirectory(third_party/embree)

# Include the packages in vcpkg or system
find_package(Eigen3    CONFIG REQUIRED)
find_package(fmt       CONFIG REQUIRED)
find_package(imgui     CONFIG REQUIRED)
find_package(imguizmo  CONFIG REQUIRED)
find_package(implot    CONFIG REQUIRED)
find_package(meshoptimizer CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(NLopt     CONFIG REQUIRED)
find_package(OpenMP    REQUIRED)
find_package(Qhull     CONFIG REQUIRED)
find_package(tinyexr   CONFIG REQUIRED)
find_package(tinyfiledialogs CONFIG REQUIRED)
find_package(Tracy     CONFIG REQUIRED)
find_package(ZLIB      REQUIRED)

# Include third party header-only packages in vcpkg
find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")
find_path(ZSTR_INCLUDE_DIRS "zstr.hpp")

# Add target metameric_shaders; compiles and copies glsl to spirv 
# from /resources/shaders to /bin/shaders
met_add_shader_target(metameric_shaders 
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders
)

# Add target metameric_data_copy; copies contents of /resources/data to /bin/data
met_add_folder_copy_target(metameric_data_copy 
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/data
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data
)

# Add target metameric_scene_copy; copies contents  of /scene/cornell_box to /bin/data. 
# This scene is loaded on program startup for demo purposes
met_add_folder_copy_target(metameric_scene_copy 
  ${CMAKE_CURRENT_SOURCE_DIR}/scenes/cornell_box
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data
)

# Set up core library; precompile core headers
met_add_library(core ${STB_INCLUDE_DIRS} ${ZSTR_INCLUDE_DIRS})
met_add_pch(core)
target_link_libraries(core 
  PUBLIC autodiff::autodiff
         Eigen3::Eigen
         fmt::fmt-header-only
         nlohmann_json::nlohmann_json
         NLopt::nlopt
         meshoptimizer::meshoptimizer
         Qhull::qhullcpp Qhull::qhull_r
         unofficial::tinyexr::tinyexr
         Tracy::TracyClient
         OpenMP::OpenMP_CXX
         ZLIB::ZLIB
)

# Set up scene library
met_add_library(scene "")
met_add_pch(scene)
met_reuse_pch(scene "core;small_gl")
target_link_libraries(scene PUBLIC core embree small_gl rapidobj)

# Set up render library
met_add_library(render "")
met_add_pch(render)
met_reuse_pch(render "core;scene")
add_dependencies(render metameric_shaders)
target_link_libraries(render PUBLIC scene)

# Set up editor library
met_add_library(editor "")
met_add_pch(editor)
met_reuse_pch(editor "core;scene;render")
add_dependencies(editor metameric_shaders)
target_link_libraries(editor 
  PUBLIC render 
         imgui::imgui
         imguizmo::imguizmo
         implot::implot
         tinyfiledialogs::tinyfiledialogs
)

# Setup editor executable
add_executable(metameric_editor src/app/metameric_editor.cpp)
add_dependencies(metameric_editor metameric_scene_copy metameric_data_copy embree_copy)
target_compile_features(metameric_editor PUBLIC cxx_std_23)
target_link_libraries(metameric_editor PRIVATE core scene render editor)