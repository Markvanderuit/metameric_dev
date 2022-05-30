cmake_minimum_required(VERSION 3.22)

# Specify vcpkg toolchain file
# set(VCPKG_TARGET_TRIPLET x64-mingw-static CACHE STRING "VCPKG Target Triplet to use")
set(CMAKE_TOOLCHAIN_FILE 
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake
  CACHE STRING "Vcpkg toolchain file")
  
project(Metameric LANGUAGES CXX)

# Specify output directories as /dist
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

# Specify resource directory name
set(MET_RESOURCE_DIRECTORY "resources")
set(MET_RUNTIME_RESOURCE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${APPL_RESOURCE_DIRECTORY}")

# Include third party libraries provided through vcpkg
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(HighFive CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb.h")
find_package(TBB CONFIG REQUIRED)

# Include third party libraries provided directly
add_subdirectory(third_party/small_gl)

# Find glslLangValidator program on system
find_program(GLSL_VALIDATOR glslangValidator HINTS 
  /usr/bin
  /usr/local/bin
  $ENV{VULKAN_SDK}/Bin/
  $ENV{VULKAN_SDK}/Bin32/
)

# Quickly find all shader sources
file(GLOB_RECURSE glsl_srcs
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*.geom
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*.comp
)

# Add commands to copy/compile shader sources
foreach(glsl_src ${glsl_srcs})
  # Obtain path, stripped of lead, and filename
  cmake_path(RELATIVE_PATH glsl_src
    BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE relative_path)
  message(STATUS "Found shader : ${relative_path}")

  set(shader_spirv_out "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${relative_path}.spv")
  set(shader_copy_out "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${relative_path}")

  # Add command to compile SPIR-V binary using glslangValidator if installed
  add_custom_command(
    OUTPUT ${shader_spirv_out}
    COMMAND ${GLSL_VALIDATOR} -G ${glsl_src} -o ${shader_spirv_out}
    DEPENDS ${glsl_src}
  )

  # Add command to simply copy shader source
  add_custom_command(
    OUTPUT ${shader_copy_out}
    COMMAND ${CMAKE_COMMAND} -E copy ${glsl_src} ${shader_copy_out}
    DEPENDS ${glsl_src}
  )

  list(APPEND shader_spirv_list ${shader_spirv_out})
  list(APPEND shader_copy_list ${shader_copy_out})
endforeach()

# Add targets to perform compilation/copy
add_custom_target(shaders_spirv DEPENDS ${shader_spirv_list})
add_custom_target(shaders_copy DEPENDS ${shader_copy_list})

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
add_custom_target(misc_copy DEPENDS ${misc_copy_list})


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
  fmt::fmt-header-only
  TBB::tbb
  glm::glm
  libdeps HighFive HighFive_libdeps HighFive_HighFive # highfive libs
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
add_dependencies(gui shaders_copy shaders_spirv misc_copy)

# Set up main executable
add_executable_r(metameric src/metameric.cpp)
target_link_libraries(metameric PRIVATE gui)

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