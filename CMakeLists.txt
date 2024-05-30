cmake_minimum_required(VERSION 3.22)

# Specify VCPKG toolchain path, CMAKE module path
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/resources" CACHE STRING "CMake module path")

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

# Gather all settings into one list
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

# Specify module path and add custom modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/resources/cmake")
include(spirv)

# Ensure these are available for third parties
find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)

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

# Find included third party shader-related tools
if (WIN32)
  set(spv_tools_path ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv/windows)
elseif(UNIX)
  set(spv_tools_path ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv/linux)
else()
  message(FATAL_ERROR "SPIR-V tools do not support this platform" )
endif()
find_program(glslangValidator NAMES glslangValidator PATHS ${spv_tools_path} NO_DEFAULT_PATH REQUIRED)
find_program(spirv-opt        NAMES spirv-opt        PATHS ${spv_tools_path} NO_DEFAULT_PATH REQUIRED)
find_program(spirv-cross      NAMES spirv-cross      PATHS ${spv_tools_path} NO_DEFAULT_PATH REQUIRED)

# Include small_gl library; forward exceptions/tracy options
option(gl_enable_exceptions  "" ${met_enable_exceptions})
option(gl_enable_tracy       "" ${met_enable_tracy})
add_subdirectory(third_party/small_gl)

# Include xAtlas/Autodiff
add_subdirectory(third_party/xatlas)
add_subdirectory(third_party/autodiff)

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

# Add single target to perform all shader preprocessing/validation
set(spirv_dependencies ${glsl_includes})
compile_glsl_to_spirv_list("${glsl_srcs}" "${spirv_dependencies}")
add_custom_target(shaders DEPENDS ${spirv_dependencies})

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

  if(OpenMP_CXX_FOUND)
    target_link_libraries(${target} PUBLIC OpenMP::OpenMP_CXX)
  endif()
  if(met_enable_exceptions)
    target_compile_definitions(${target} PUBLIC MET_ENABLE_EXCEPTIONS)
  endif()
  if(met_enable_tracy)
    target_compile_definitions(${target} PUBLIC MET_ENABLE_TRACY)
  endif()
endfunction()

# Simple function to add an executable with consistent settings
function(add_executable_r target src)
  add_executable(${target} ${src})
  target_compile_features(${target} PUBLIC cxx_std_23)
  if(OpenMP_CXX_FOUND)
    target_link_libraries(${target} PUBLIC OpenMP::OpenMP_CXX)
  endif()
  if(met_enable_exceptions)
    target_compile_definitions(${target} PUBLIC MET_ENABLE_EXCEPTIONS)
  endif()
  if(met_enable_tracy)
    target_compile_definitions(${target} PUBLIC MET_ENABLE_TRACY)
  endif()
endfunction()

# Set up core library
add_library_r(core core)
target_link_libraries(core PUBLIC
  autodiff::autodiff
  assimp::assimp
  Eigen3::Eigen
  embree
  fmt::fmt-header-only
  nlohmann_json::nlohmann_json
  NLopt::nlopt
  meshoptimizer::meshoptimizer
  Qhull::qhullcpp
  Qhull::qhull_r
  unofficial::tinyexr::tinyexr
  small_gl
  TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy
  Tracy::TracyClient
  xatlas
  ZLIB::ZLIB
)
target_include_directories(core PUBLIC ${STB_INCLUDE_DIRS} ${ZSTR_INCLUDE_DIRS})
target_compile_definitions(core PUBLIC _USE_MATH_DEFINES ${preprocessor_defines})

# Set up render library
add_library_r(render render)
target_link_libraries(render PUBLIC
  core
  imgui::imgui
)

# Set up components library
add_library_r(components components)
target_link_libraries(components PUBLIC
  render
  imgui::imgui
  imguizmo::imguizmo
  implot::implot
  unofficial::nativefiledialog::nfd
)

# Link dependencies for copying/compiling resources
add_dependencies(core       misc_copy)
add_dependencies(render     shaders)
add_dependencies(components shaders)

# Setup executables
add_executable_r(metameric_editor src/app/metameric_editor.cpp)
target_link_libraries(metameric_editor PRIVATE core render components)
add_executable_r(moment_testing src/app/moment_testing.cpp)
target_link_libraries(moment_testing PRIVATE core render components)
add_executable_r(noise_testing src/app/noise_testing.cpp)
target_link_libraries(noise_testing PRIVATE core render components)

# Optionally setup Catch2 tests
if (met_enable_tests)
  include(CTest)
  include(Catch)

  # Find all test files
  file(GLOB_RECURSE srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)

  # Setup test executable
  add_executable(metameric_tests ${srcs})
  target_link_libraries(metameric_tests PRIVATE Catch2::Catch2WithMain core)
  target_compile_features(metameric_tests PUBLIC cxx_std_23)
  catch_discover_tests(metameric_tests WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()