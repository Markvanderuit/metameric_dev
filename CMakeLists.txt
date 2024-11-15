cmake_minimum_required(VERSION 3.22)

# Specify VCPKG toolchain path, CMAKE module path
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/resources/cmake" CACHE STRING "CMake module path")

# Specify project name, languages
project(Metameric LANGUAGES CXX)

# Build options
option(MET_ENABLE_EXCEPTIONS "Enable exceptions on release" ON)
option(MET_ENABLE_TRACY      "Enable Tracy support"        OFF)
option(MET_ENABLE_TESTS      "Build tests"                  ON)

# Spectral layout; inserted in code/shaders as preprocessor defines
set(MET_WAVELENGTH_MIN     380) # Minimum wavelength of spectral range
set(MET_WAVELENGTH_MAX     780) # Maximum wavelength of spectral range
set(MET_WAVELENGTH_SAMPLES  64) # Nr. of bins used in discrete spectral representations
set(MET_WAVELENGTH_BASES    12) # Maximum nr. of bases used in generative functions

# Some object limits for renderer; inserted in code/shaders as preprocessor defines
set(MET_SUPPORTED_MESHES        32) # Maximum supported scene meshes
set(MET_SUPPORTED_OBJECTS       32) # Maximum supported scene objects
set(MET_SUPPORTED_EMITTERS      32) # Maximum supported scene emitters
set(MET_SUPPORTED_UPLIFTINGS     8) # Maximum supported uplifting structures; multiple objects can share an uplifting
set(MET_SUPPORTED_CONSTRAINTS 1024) # Maximum supported boundary spectra + constraint spectra
set(MET_SUPPORTED_TEXTURES     128) # Maximum supported scene textures

# TODO remove; inserted in code/shaders as preprocessor defines
set(MET_MOMENT_COEFFICIENTS 12) # Nr. of coeffs used for moment-based spectral representation

# Print configuration info
message(STATUS "Metameric : Enabling exceptions = ${MET_ENABLE_EXCEPTIONS}")
message(STATUS "Metameric : Enabling Tracy      = ${MET_ENABLE_TRACY}")
message(STATUS "Metameric : Building tests      = ${MET_ENABLE_TESTS}")
message(STATUS "Metameric : Wavelength min.     = ${MET_WAVELENGTH_MIN}")
message(STATUS "Metameric : Wavelength max.     = ${MET_WAVELENGTH_MAX}")
message(STATUS "Metameric : Wavelength samples  = ${MET_WAVELENGTH_SAMPLES}")
message(STATUS "Metameric : Wavelength bases    = ${MET_WAVELENGTH_BASES}")

# Specify output directories as /bin and /lib
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Ensure these are available for third parties
find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)

# Include xAtlas/Autodiff/small_gl manually
option(gl_enable_exceptions  "" ${MET_ENABLE_EXCEPTIONS})
option(gl_enable_tracy       "" ${MET_ENABLE_TRACY})
add_subdirectory(third_party/autodiff)
add_subdirectory(third_party/embree)
add_subdirectory(third_party/rapidobj)
add_subdirectory(third_party/small_gl)
add_subdirectory(third_party/xatlas)

# Include third party libraries
find_package(Catch2    CONFIG REQUIRED)
find_package(Eigen3    CONFIG REQUIRED)
find_package(fmt       CONFIG REQUIRED)
find_package(imgui     CONFIG REQUIRED)
find_package(imguizmo  CONFIG REQUIRED)
find_package(implot    CONFIG REQUIRED)
find_package(meshoptimizer CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(NLopt     CONFIG REQUIRED)
find_package(Qhull     CONFIG REQUIRED)
find_package(tinyexr   CONFIG REQUIRED)
find_package(Tracy     CONFIG REQUIRED)
find_package(nfd       CONFIG REQUIRED)
find_package(ZLIB      REQUIRED)
find_package(embree    CONFIG REQUIRED)

# Include third party header-only libraries provided through vcpkg
find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")
find_path(ZSTR_INCLUDE_DIRS "zstr.hpp")

# Specify cmake modules/finds
include(CTest)
include(Catch)
include(spirv)
include(resource)

# Gather all preprocessor values into one list; these are passed to cxx and glsl
set(preprocessor_defines
  -DMET_WAVELENGTH_MIN=${MET_WAVELENGTH_MIN} 
  -DMET_WAVELENGTH_MAX=${MET_WAVELENGTH_MAX} 
  -DMET_WAVELENGTH_SAMPLES=${MET_WAVELENGTH_SAMPLES} 
  -DMET_WAVELENGTH_BASES=${MET_WAVELENGTH_BASES} 
  -DMET_MOMENT_COEFFICIENTS=${MET_MOMENT_COEFFICIENTS}
  -DMET_SUPPORTED_MESHES=${MET_SUPPORTED_MESHES}
  -DMET_SUPPORTED_OBJECTS=${MET_SUPPORTED_OBJECTS}
  -DMET_SUPPORTED_EMITTERS=${MET_SUPPORTED_EMITTERS}
  -DMET_SUPPORTED_UPLIFTINGS=${MET_SUPPORTED_UPLIFTINGS}
  -DMET_SUPPORTED_CONSTRAINTS=${MET_SUPPORTED_CONSTRAINTS}
  -DMET_SUPPORTED_TEXTURES=${MET_SUPPORTED_TEXTURES}
)
if(MET_ENABLE_EXCEPTIONS)
  list(APPEND preprocessor_defines -DMET_ENABLE_EXCEPTIONS)
endif()
if(MET_ENABLE_TRACY)
  list(APPEND preprocessor_defines -DMET_ENABLE_TRACY)
endif()

# Gather list of third party libraries
set(thrp_includes ${STB_INCLUDE_DIRS} ${ZSTR_INCLUDE_DIRS})
set(core_dependencies
  autodiff::autodiff
  Eigen3::Eigen
  embree
  fmt::fmt-header-only
  nlohmann_json::nlohmann_json
  NLopt::nlopt
  meshoptimizer::meshoptimizer
  Qhull::qhullcpp Qhull::qhull_r
  unofficial::tinyexr::tinyexr
  rapidobj
  small_gl
  Tracy::TracyClient
  xatlas
  ZLIB::ZLIB
)
set(component_dependencies
  imgui::imgui
  imguizmo::imguizmo
  implot::implot
  nfd::nfd
)
if(OpenMP_CXX_FOUND)
  list(APPEND core_dependencies OpenMP::OpenMP_CXX)
endif()

# Specify precompiled header file
function(add_library_pch target)
  file(GLOB_RECURSE incls ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)
  target_precompile_headers(${target}
    PUBLIC ${incls}
    PRIVATE <array> <algorithm> <deque> <iostream> <filesystem> <vector> <ranges> <unordered_map> <unordered_set>
  )
endfunction()

# Simple function to add a library with consistent settings
function(add_library_r target dir)
  # Gather source files recursively
  file(GLOB_RECURSE srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/*.cpp)

  # Specify target
  add_library(${target} ${srcs})

  # Specify target includes/features/defines
  target_include_directories(${target} PUBLIC include ${thrp_includes})
  target_compile_features(${target} PUBLIC cxx_std_23)
  target_compile_definitions(${target} PUBLIC _USE_MATH_DEFINES ${preprocessor_defines})

  # Specify unity build, batching 8 files per object
  # set_target_properties(${target} PROPERTIES 
  #   UNITY_BUILD ON 
  #   UNITY_BUILD_MODE BATCH 
  #   UNITY_BUILD_BATCH_SIZE 4
  # )

  # Enable /bigobj (or the closest thing for gcc)
  # Src: https://stackoverflow.com/questions/69903253/cmake-bigobj-no-such-file-or-directory
  if (MSVC)
    target_compile_options(${target} PRIVATE /bigobj)
  else ()
    target_compile_options(${target} PRIVATE -Wa,-mbig-obj)
  endif ()
endfunction()

# Set up core library
add_library_r(core core)
target_link_libraries(core PUBLIC ${core_dependencies})
add_library_pch(core)

# Set up render library
add_library_r(render render)
target_link_libraries(render PUBLIC core)
target_precompile_headers(render REUSE_FROM core)

# Set up components library
add_library_r(components components)
target_link_libraries(components PUBLIC render ${component_dependencies})
target_precompile_headers(components REUSE_FROM core)

# Ignore unity builds on scene.cpp/math.cpp
# as some dependency seems to have an incompatibility for now
# set_source_files_properties(
#   ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scene.cpp 
#   ${CMAKE_CURRENT_SOURCE_DIR}/src/core/math.cpp 
#   PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
# )

# Specify shader/resource targets, link these as dependencies for preprocessing
add_shader_target(shaders shaders.bin)
add_resource_target(resources misc)
add_dependencies(core       resources)
add_dependencies(render     shaders)
add_dependencies(components shaders)

# Setup editor executable
add_executable(metameric_editor src/app/metameric_editor.cpp)
target_compile_features(metameric_editor PUBLIC cxx_std_23)
target_link_libraries(metameric_editor PRIVATE core render components)

# Setup render executable
find_package(avcpp CONFIG REQUIRED)
add_executable(ffw_video src/app/ffw_video.cpp)
target_compile_features(ffw_video PUBLIC cxx_std_23)
target_link_libraries(ffw_video PRIVATE core render components avcpp::FFmpeg avcpp::avcpp)
target_include_directories(ffw_video PRIVATE src/app/include)
add_executable(presentation_videos src/app/presentation_videos.cpp)
target_compile_features(presentation_videos PUBLIC cxx_std_23)
target_link_libraries(presentation_videos PRIVATE core render components avcpp::FFmpeg avcpp::avcpp)
target_include_directories(presentation_videos PRIVATE src/app/include)

# Optionally setup Catch2 tests
if (MET_ENABLE_TESTS)
  # Find all test files
  file(GLOB_RECURSE srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)

  # Setup test executable
  add_executable(metameric_tests ${srcs})
  target_link_libraries(metameric_tests PRIVATE Catch2::Catch2WithMain core)
  target_compile_features(metameric_tests PUBLIC cxx_std_23)
  catch_discover_tests(metameric_tests WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

# Finally, handle dependency copy if necessary
add_custom_command(
  TARGET metameric_editor POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:metameric_editor> $<TARGET_FILE_DIR:metameric_editor> COMMAND_EXPAND_LISTS
)

# Copy tbb12 so embree doesn't break things
if (WIN32)
  add_custom_command(
    TARGET metameric_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${TBB12_LIBRARY}
    $<TARGET_FILE_DIR:metameric_editor>/tbb12.dll
  )
endif()