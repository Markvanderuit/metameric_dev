cmake_minimum_required(VERSION 3.22)

# Specify vcpkg toolchain path
set(CMAKE_TOOLCHAIN_FILE 
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake
  CACHE STRING "Vcpkg toolchain file")

# Specify CMake modules path
set(CMAKE_MODULE_PATH 
  ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/resources"
  CACHE STRING "CMake module path")

project(Metameric LANGUAGES CXX)

# Define Metameric's spectral layout
set(MET_WAVELENGTH_MIN     400)
set(MET_WAVELENGTH_MAX     700)
set(MET_WAVELENGTH_SAMPLES  32)

# Specify output directories as /bin and /lib
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Punch MSVC's multi-configuration output directories until they follow suit
if(MSVC)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG          ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG          ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG          ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE        ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL     ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL     ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
endif(MSVC)

# Ensure OpenMP is available
find_package(OpenMP REQUIRED)

# Include third party libraries provided through vcpkg
find_package(benchmark CONFIG REQUIRED)
find_package(Eigen3    CONFIG REQUIRED)
find_package(fmt       CONFIG REQUIRED)
find_package(glm       CONFIG REQUIRED)
find_package(HighFive  CONFIG REQUIRED)
find_package(imgui     CONFIG REQUIRED)
find_package(imguizmo  CONFIG REQUIRED)
find_package(TBB       CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb.h")

# Include third party libraries provided directly
add_subdirectory(third_party/small_gl)
add_subdirectory(third_party/small_glsl_parser)

# Find glslLangValidator program on system
find_program(glsl_validator_path glslangValidator HINTS 
  /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/
)

# Recursively find all shader files
file(GLOB_RECURSE glsl_srcs
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/src/*.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/src/*.geom
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/src/*.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/src/*.comp
)
file(GLOB_RECURSE glsl_includes 
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/include/*
)

# Add together arguments to pass into glsl parser
string(CONCAT glsl_parser_arg_string
  "MET_WAVELENGTH_MIN=${MET_WAVELENGTH_MIN},"
  "MET_WAVELENGTH_MAX=${MET_WAVELENGTH_MAX},"
  "MET_WAVELENGTH_SAMPLES=${MET_WAVELENGTH_SAMPLES}"
)

# Process shaders
foreach(glsl_src ${glsl_srcs})
  # Obtain path, stripped of lead, and filename
  cmake_path(RELATIVE_PATH glsl_src
    BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/src
    OUTPUT_VARIABLE relative_path)
  message(STATUS "Found shader : ${relative_path}")

  set(shader_preprocess_out 
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/${relative_path}")

  # Add command to preprocess shader source using glsl parser and 
  # then pass it through glslangvalidator, if available
  add_custom_command(
    OUTPUT   ${shader_preprocess_out}
    COMMAND  small_glsl_parser_exec 
               ${glsl_src} ${shader_preprocess_out} 
               -p "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/include"
               -s "${glsl_parser_arg_string}"
               -v "${glsl_validator_path}"
    DEPENDS  small_glsl_parser_exec ${glsl_src} ${glsl_includes}
    VERBATIM
  )

  list(APPEND shaders_preprocess_list ${shader_preprocess_out})
endforeach()

# Add single target to perform all shader preprocessing/validation
add_custom_target(shaders_preprocess DEPENDS ${shaders_preprocess_list})

# Add commands to copy resources/misc files
file(GLOB_RECURSE misc_files ${CMAKE_CURRENT_SOURCE_DIR}/resources/misc/*)
foreach(misc_file ${misc_files})
  # Obtain path, stripped of lead, and filename
  cmake_path(RELATIVE_PATH misc_file
    BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE relative_path)
  message(STATUS "Found misc resource : ${relative_path}")

  set(misc_out "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${relative_path}")
  add_custom_command(
    OUTPUT ${misc_out}
    COMMAND ${CMAKE_COMMAND} -E copy ${misc_file} ${misc_out}
    DEPENDS ${misc_file}
  )
  list(APPEND misc_copy_list ${misc_out})
endforeach()

# Add single target to perfrom all resource/misc file copies
add_custom_target(misc_copy DEPENDS ${misc_copy_list})

# Simple function to add a library with consistent settings
function(add_library_r target dir)
  file(GLOB_RECURSE srcs 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/metameric/${dir}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/metameric/${dir}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/*.cpp
  )
  add_library(${target} ${srcs})
  target_include_directories(${target} PUBLIC include)
  target_compile_features(${target} PUBLIC cxx_std_23)
endfunction()

# Simple function to add an executable with consistent settings
function(add_executable_r target src)
  add_executable(${target} ${src})
  target_compile_features(${target} PUBLIC cxx_std_23)
endfunction()

# Set up core library
add_library_r(core core)
target_link_libraries(core PUBLIC
  small_gl
  Eigen3::Eigen
  fmt::fmt-header-only
  TBB::tbb
  glm::glm
  libdeps HighFive HighFive_libdeps HighFive_HighFive # highfive libs
)
target_include_directories(core PRIVATE ${STB_INCLUDE_DIRS})
target_compile_definitions(core PUBLIC
  MET_WAVELENGTH_MIN=${MET_WAVELENGTH_MIN}
  MET_WAVELENGTH_MAX=${MET_WAVELENGTH_MAX}
  MET_WAVELENGTH_SAMPLES=${MET_WAVELENGTH_SAMPLES}
)

# Set up gui library
add_library_r(gui gui)
target_link_libraries(gui PUBLIC
  core
  small_gl
  imgui::imgui
  imguizmo::imguizmo
)
add_dependencies(gui shaders_preprocess misc_copy)

# Set up metameric executable
add_executable_r(metameric src/metameric.cpp)
target_link_libraries(metameric PRIVATE gui)

# Set up benchmark executable
add_executable_r(benchmark src/main_benchmark.cpp)
target_link_libraries(benchmark PRIVATE 
  core
  benchmark::benchmark
  benchmark::benchmark_main
  Eigen3::Eigen
)

# Set up playground executable
add_executable_r(main_playground src/main_playground.cpp)
target_link_libraries(main_playground PRIVATE
  core
  small_gl
  fmt::fmt-header-only
  imgui::imgui
  imguizmo::imguizmo
  TBB::tbb
)