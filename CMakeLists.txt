cmake_minimum_required(VERSION 3.22)

# Specify VCPKG toolchain path, CMAKE module path
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/resources" CACHE STRING "CMake module path")

project(Metameric LANGUAGES CXX)

# Build options
option(met_enable_dbg_exceptions "Enable debug exceptions on release" ON)
option(met_enable_tracy          "Enable Tracy support"               ON)

# Define Metameric's spectral layout
set(MET_WAVELENGTH_MIN     380) # Minimum wavelength of spectral range
set(MET_WAVELENGTH_MAX     780) # Maximum wavelength of spectral range
set(MET_WAVELENGTH_SAMPLES  64) # Nr. of bins used in discrete spectral representations
set(MET_WAVELENGTH_BASES    12) # Nr. of basis functions used in generative functions
set(MET_GENERALIZED_WEIGHTS 16) # Maximum nr. of generalized convex weights supported

# Print configuration info
message(STATUS "Metameric : Enabling exceptions = ${met_enable_dbg_exceptions}")
message(STATUS "Metameric : Enabling Tracy      = ${met_enable_tracy}")
message(STATUS "Metameric : Wavelength min.     = ${MET_WAVELENGTH_MIN}")
message(STATUS "Metameric : Wavelength max.     = ${MET_WAVELENGTH_MAX}")
message(STATUS "Metameric : Wavelength samples  = ${MET_WAVELENGTH_SAMPLES}")
message(STATUS "Metameric : Wavelength bases    = ${MET_WAVELENGTH_BASES}")
message(STATUS "Metameric : Generalized weights = ${MET_GENERALIZED_WEIGHTS}")

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
find_package(CoinUtils CONFIG REQUIRED)
find_package(Osi       CONFIG REQUIRED)
find_package(Clp       CONFIG REQUIRED)
find_package(Eigen3    CONFIG REQUIRED)
find_package(fmt       CONFIG REQUIRED)
find_package(imgui     CONFIG REQUIRED)
find_package(imguizmo  CONFIG REQUIRED)
find_package(implot    CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(openmesh  CONFIG REQUIRED)
find_package(Qhull     CONFIG REQUIRED)
find_package(tinyexr   CONFIG REQUIRED)
find_package(Tracy     CONFIG REQUIRED)
find_package(unofficial-nativefiledialog CONFIG REQUIRED)
find_package(ZLIB      REQUIRED)

# Include header-only third party libraries provided through vcpkg
find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")
find_path(ZSTR_INCLUDE_DIRS "zstr.hpp")

# Include third party shader-related tools provided through vcpkg
find_program(glslangValidator NAMES glslangValidator PATHS ${CMAKE_PROGRAM_PATH} REQUIRED)
find_program(spirv-opt        NAMES spirv-opt        PATHS ${CMAKE_PROGRAM_PATH} REQUIRED)
find_program(spirv-cross      NAMES spirv-cross      PATHS ${CMAKE_PROGRAM_PATH} REQUIRED)

# Include small_gl library directly; forward exceptions/tracy options
option(gl_enable_dbg_exceptions "" ${met_enable_dbg_exceptions})
option(gl_enable_tracy          "" ${met_enable_tracy})
add_subdirectory(third_party/small_gl)

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
string(CONCAT small_gl_parser_args
  "MET_WAVELENGTH_MIN=${MET_WAVELENGTH_MIN},"
  "MET_WAVELENGTH_MAX=${MET_WAVELENGTH_MAX},"
  "MET_WAVELENGTH_SAMPLES=${MET_WAVELENGTH_SAMPLES},"
  "MET_WAVELENGTH_BASES=${MET_WAVELENGTH_BASES},"
  "MET_GENERALIZED_WEIGHTS=${MET_GENERALIZED_WEIGHTS}"
)

# Process shaders
foreach(glsl_src ${glsl_srcs})
  # Obtain path, stripped of lead, and filename
  cmake_path(RELATIVE_PATH glsl_src
             BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/src
             OUTPUT_VARIABLE relative_path)

  message(STATUS "Found shader : ${relative_path}")
  
  set(spirv_parse_fp "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/${relative_path}")
  set(spirv_temp_fp  "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/${relative_path}.temp")
  set(spirv_bin_fp   "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/${relative_path}.spv")
  set(spirv_refl_fp  "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/${relative_path}.json")
  
  # Preprocess shader includes and preprocess strings using small_gl_parser
  add_custom_command(
    OUTPUT  ${spirv_parse_fp}
    COMMAND small_gl_parser_exec ${glsl_src} ${spirv_parse_fp} 
            -p "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/include"
            -s "${small_gl_parser_args}"
    DEPENDS small_gl_parser_exec ${glsl_src} ${glsl_includes}
    VERBATIM
  )

  # Generate spir-v binary using glslangValidator, store in .temp file
  add_custom_command(
    OUTPUT  ${spirv_temp_fp}
    COMMAND ${glslangValidator} ${spirv_parse_fp} 
            -o ${spirv_temp_fp} --client opengl100 --target-env spirv1.5 -l
    DEPENDS ${spirv_parse_fp}
    VERBATIM
  )

  # Generate optimized spir-v binary using spirv-opt from spirv-tools
  add_custom_command(
    OUTPUT  ${spirv_bin_fp}
    COMMAND ${spirv-opt} ${spirv_temp_fp} 
            -o ${spirv_bin_fp} -O --strength-reduction --combine-access-chains
    DEPENDS ${spirv_temp_fp}
    VERBATIM
  )

  # Generate spir-v reflection information in .json files using spirv-cross
  add_custom_command(
    OUTPUT  ${spirv_refl_fp}
    COMMAND ${spirv-cross} ${spirv_temp_fp} 
            --output ${spirv_refl_fp} --reflect
    DEPENDS ${spirv_temp_fp}
    VERBATIM
  )

  # Set up list of dependency commands for outputs
  list(APPEND shader_dependencies ${spirv_bin_fp} ${spirv_refl_fp})
endforeach()

# Add single target to perform all shader preprocessing/validation
add_custom_target(shaders DEPENDS ${shader_dependencies})

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
  small_gl_parser_lib
  Eigen3::Eigen
  fmt::fmt-header-only
  Coin::Clp
  Tracy::TracyClient
  nlohmann_json::nlohmann_json
  unofficial::tinyexr::tinyexr
  ${OPENMESH_LIBRARIES}
  Qhull::qhullcpp
  Qhull::qhull_r
  ZLIB::ZLIB
)
target_include_directories(core PUBLIC
  ${STB_INCLUDE_DIRS}
  ${OPENMESH_INCLUDE_DIRS}
  ${ZSTR_INCLUDE_DIRS}
)
target_compile_definitions(core PUBLIC
  MET_WAVELENGTH_MIN=${MET_WAVELENGTH_MIN}
  MET_WAVELENGTH_MAX=${MET_WAVELENGTH_MAX}
  MET_WAVELENGTH_SAMPLES=${MET_WAVELENGTH_SAMPLES}
  MET_WAVELENGTH_BASES=${MET_WAVELENGTH_BASES}
  MET_GENERALIZED_WEIGHTS=${MET_GENERALIZED_WEIGHTS}
)
target_compile_definitions(core PUBLIC _USE_MATH_DEFINES) # exposec cmath defines in msvc

# Set up components library
add_library_r(components components)
target_link_libraries(components PUBLIC
  core
  small_gl
  imgui::imgui
  imguizmo::imguizmo
  implot::implot
  unofficial::nativefiledialog::nfd
)
add_dependencies(components shaders misc_copy)

# Set up app library
add_library_r(app app)
target_link_libraries(app PUBLIC
  core
  components
  small_gl
)

# Set up metameric executable
add_executable_r(metameric src/target/main_metameric.cpp)
target_link_libraries(metameric PRIVATE app)

add_executable_r(scheduler src/target/main_scheduler.cpp)
target_link_libraries(scheduler PRIVATE app)

# Configure OpenMP
if(OpenMP_CXX_FOUND)
    target_link_libraries(core       PUBLIC OpenMP::OpenMP_CXX)
    target_link_libraries(components PUBLIC OpenMP::OpenMP_CXX)
    target_link_libraries(app        PUBLIC OpenMP::OpenMP_CXX)
    target_link_libraries(metameric  PUBLIC OpenMP::OpenMP_CXX)
endif()

# Configure debug output options
if (met_enable_dbg_exceptions)
  target_compile_definitions(core       PUBLIC MET_ENABLE_DBG_EXCEPTIONS)
  target_compile_definitions(components PUBLIC MET_ENABLE_DBG_EXCEPTIONS)
  target_compile_definitions(app        PUBLIC MET_ENABLE_DBG_EXCEPTIONS)
  target_compile_definitions(metameric  PUBLIC MET_ENABLE_DBG_EXCEPTIONS)
endif()

# Configure tracy integration options
if (met_enable_tracy)
  target_compile_definitions(core       PUBLIC MET_ENABLE_TRACY)
  target_compile_definitions(components PUBLIC MET_ENABLE_TRACY)
  target_compile_definitions(app        PUBLIC MET_ENABLE_TRACY)
  target_compile_definitions(metameric  PUBLIC MET_ENABLE_TRACY)
endif()