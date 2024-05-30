cmake_minimum_required(VERSION 3.22)

# Specify VCPKG toolchain path, CMAKE module path
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/resources/cmake" CACHE STRING "CMake module path")

project(Metameric LANGUAGES CXX)

# Build options
option(met_enable_exceptions "Enable exceptions on release" ON)
option(met_enable_tracy      "Enable Tracy support"         ON)
option(met_enable_tests      "Build tests"                  ON)

# Define Metameric's spectral layout
set(MET_WAVELENGTH_MIN     380) # Minimum wavelength of spectral range
set(MET_WAVELENGTH_MAX     780) # Maximum wavelength of spectral range
set(MET_WAVELENGTH_SAMPLES  64) # Nr. of bins used in discrete spectral representations
set(MET_WAVELENGTH_BASES    12) # Maximum nr. of bases used in generative functions

# TODO remove
set(MET_MOMENT_COEFFICIENTS 12) # Nr. of coeffs used for moment-based spectral representation

# Define some constants as available on both shader and program side
set(MET_SUPPORTED_MESHES        48) # Maximum supported scene meshes
set(MET_SUPPORTED_OBJECTS       48) # Maximum supported scene objects
set(MET_SUPPORTED_EMITTERS      16) # Maximum supported scene emitters
set(MET_SUPPORTED_UPLIFTINGS     8) # Maximum supported individual uplifting textures
set(MET_SUPPORTED_CONSTRAINTS 1024) # Maximum supported boundary spectra + constraint spectra
set(MET_SUPPORTED_TEXTURES     256) # Maximum supported scene textures

# Print configuration info
message(STATUS "Metameric : Enabling exceptions = ${met_enable_exceptions}")
message(STATUS "Metameric : Enabling Tracy      = ${met_enable_tracy}")
message(STATUS "Metameric : Building tests      = ${met_enable_tests}")
message(STATUS "Metameric : Wavelength min.     = ${MET_WAVELENGTH_MIN}")
message(STATUS "Metameric : Wavelength max.     = ${MET_WAVELENGTH_MAX}")
message(STATUS "Metameric : Wavelength samples  = ${MET_WAVELENGTH_SAMPLES}")
message(STATUS "Metameric : Wavelength bases    = ${MET_WAVELENGTH_BASES}")

# Specify output directories as /bin and /lib
# Punch MSVC's multi-configuration output directories until they follow suit
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
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

# Ensure these are available for third parties
find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)

# Gather all preprocessor values into one list
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
if(met_enable_exceptions)
  list(APPEND preprocessor_defines -DMET_ENABLE_EXCEPTIONS)
endif()
if(met_enable_tracy)
  list(APPEND preprocessor_defines -DMET_ENABLE_TRACY)
endif()

# Include third party libraries provided through vcpkg
find_package(assimp    CONFIG REQUIRED)
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
find_package(TBB       CONFIG REQUIRED)
find_package(tinyexr   CONFIG REQUIRED)
find_package(Tracy     CONFIG REQUIRED)
find_package(unofficial-nativefiledialog CONFIG REQUIRED)
find_package(ZLIB      REQUIRED)
find_package(embree    CONFIG REQUIRED)

# Include third party header-only libraries provided through vcpkg
find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")
find_path(ZSTR_INCLUDE_DIRS "zstr.hpp")

# Include xAtlas/Autodiff/small_gl
option(gl_enable_exceptions  "" ${met_enable_exceptions})
option(gl_enable_tracy       "" ${met_enable_tracy})
add_subdirectory(third_party/xatlas)
add_subdirectory(third_party/autodiff)
add_subdirectory(third_party/small_gl)

# Specify cmake modules/finds
include(CTest)
include(Catch)
include(spirv)
include(resource)

# Gather list of third party libraries
set(core_dependencies
  autodiff::autodiff
  assimp::assimp
  Eigen3::Eigen
  embree
  fmt::fmt-header-only
  nlohmann_json::nlohmann_json
  NLopt::nlopt
  meshoptimizer::meshoptimizer
  Qhull::qhullcpp Qhull::qhull_r
  unofficial::tinyexr::tinyexr
  small_gl
  TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy
  Tracy::TracyClient
  xatlas
  ZLIB::ZLIB
)
set(component_dependencies
  imgui::imgui imguizmo::imguizmo implot::implot unofficial::nativefiledialog::nfd
)
if(OpenMP_CXX_FOUND)
  list(APPEND core_dependencies OpenMP::OpenMP_CXX)
endif()

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

# Set up core library
add_library_r(core core)
target_link_libraries(core PUBLIC ${core_dependencies})
target_include_directories(core PUBLIC ${STB_INCLUDE_DIRS} ${ZSTR_INCLUDE_DIRS})
target_compile_definitions(core PUBLIC _USE_MATH_DEFINES ${preprocessor_defines})

# Set up render library
add_library_r(render render)
target_link_libraries(render PUBLIC core)

# Set up components library
add_library_r(components components)
target_link_libraries(components PUBLIC render ${component_dependencies})

# Specify shader/resource targets, link these as dependencies for preprocessing
add_shader_target(shaders)
add_resource_target(misc)
add_dependencies(core       misc)
add_dependencies(render     shaders)
add_dependencies(components shaders)

# Setup editor executable
add_executable(metameric_editor src/app/metameric_editor.cpp)
target_compile_features(metameric_editor PUBLIC cxx_std_23)
target_link_libraries(metameric_editor PRIVATE core render components)

# Optionally setup Catch2 tests
if (met_enable_tests)
  # Find all test files
  file(GLOB_RECURSE srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)

  # Setup test executable
  add_executable(metameric_tests ${srcs})
  target_link_libraries(metameric_tests PRIVATE Catch2::Catch2WithMain core)
  target_compile_features(metameric_tests PUBLIC cxx_std_23)
  catch_discover_tests(metameric_tests WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()