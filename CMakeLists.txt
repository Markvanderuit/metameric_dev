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

# Build options
option(met_enable_dbg_exceptions "Enable debug exceptions on release" ON)
option(met_enable_tracy          "Enable Tracy support"               ON)

# Define Metameric's spectral layout
set(MET_WAVELENGTH_MIN    400)
set(MET_WAVELENGTH_MAX    700)
set(MET_WAVELENGTH_SAMPLES 31)

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
find_package(CGAL      CONFIG REQUIRED)
find_package(Eigen3    CONFIG REQUIRED)
find_package(fmt       CONFIG REQUIRED)
find_package(HighFive  CONFIG REQUIRED)
find_package(imgui     CONFIG REQUIRED)
find_package(imguizmo  CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(TBB       CONFIG REQUIRED)
find_package(Tracy     CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")
find_package(unofficial-nativefiledialog CONFIG REQUIRED)

# Include third party shader-related libraries provided through vcpkg
find_package(glslang             CONFIG REQUIRED)
find_package(spirv_cross_core    CONFIG REQUIRED)
find_package(spirv_cross_glsl    CONFIG REQUIRED)
find_package(spirv_cross_util    CONFIG REQUIRED)
find_package(spirv_cross_reflect CONFIG REQUIRED)
find_package(SPIRV-Tools         CONFIG REQUIRED)
find_package(SPIRV-Tools-opt     CONFIG REQUIRED)

# Include third party small_gl library directly
option(gl_enable_dbg_exceptions "" ${met_enable_dbg_exceptions})
option(gl_enable_tracy          "" ${met_enable_tracy})
add_subdirectory(third_party/small_gl)

# Configuration output info
message(STATUS "Metameric : debug exceptions   = ${met_enable_dbg_exceptions}")
message(STATUS "Metameric : Tracy support      = ${met_enable_tracy}")
message(STATUS "Metameric : Wavelength min.    = ${MET_WAVELENGTH_MIN}")
message(STATUS "Metameric : Wavelength max.    = ${MET_WAVELENGTH_MAX}")
message(STATUS "Metameric : Wavelength samples = ${MET_WAVELENGTH_SAMPLES}")

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

# Add together placeholder args to pass into glsl parser;
# replaced at runtime with the correct values
string(CONCAT glsl_parser_placeholder_string
  "MET_SUBGROUP_SIZE=32"
)

# Define build target for query_wg_size

# Process shaders
foreach(glsl_src ${glsl_srcs})
  # Obtain path, stripped of lead, and filename
  cmake_path(RELATIVE_PATH glsl_src
    BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/src
    OUTPUT_VARIABLE relative_path)
  message(STATUS "Found shader : ${relative_path}")

  set(shader_preprocess_out 
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/${relative_path}")
  set(shader_binary_out 
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/${relative_path}.spv")
  set(shader_binary_opt_out 
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/${relative_path}.spv_opt")
  set(shader_reflect_out
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/${relative_path}.json")
  
  # Preprocess shader includes and preprocess strings
  add_custom_command(
    OUTPUT  ${shader_preprocess_out}
    COMMAND small_gl_parser_exec 
            ${glsl_src} ${shader_preprocess_out} 
            -p "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/include"
            -s "${glsl_parser_arg_string}"
            -d "${glsl_parser_placeholder_string}"
    DEPENDS small_gl_parser_exec ${glsl_src} ${glsl_includes}
    VERBATIM
  )

  # Generate spir-v binary using glslangValidator
  add_custom_command(
    OUTPUT  ${shader_binary_out}
    COMMAND glslangValidator
            ${shader_preprocess_out}
            -o ${shader_binary_out}
            --client opengl100 --target-env spirv1.5 -l
    DEPENDS glslangValidator ${shader_preprocess_out}
    VERBATIM
  )

  # Generate optimized spir-v binary using spirv-opt from spirv-tools
  add_custom_command(
    OUTPUT  ${shader_binary_opt_out}
    COMMAND spirv-opt
            ${shader_binary_out}
            -o ${shader_binary_opt_out}
            -O --strength-reduction --combine-access-chains
    DEPENDS SPIRV-Tools-opt ${shader_binary_out}
    VERBATIM
  )

  # Generate spir-v reflection information using spirv-cross
  add_custom_command(
    OUTPUT  ${shader_reflect_out}
    COMMAND spirv-cross
            ${shader_binary_out}
            --output ${shader_reflect_out}
            --reflect
    DEPENDS spirv-cross-core ${shader_binary_out}
    VERBATIM
  )

  # Set uo list of dependency commands
  list(APPEND shaders_preprocess_list 
    ${shader_preprocess_out} 
    ${shader_binary_out} 
    ${shader_binary_opt_out}
    ${shader_reflect_out}
  )
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
  small_gl_parser_lib
  Eigen3::Eigen
  fmt::fmt-header-only
  CGAL::CGAL
  Tracy::TracyClient
  libdeps HighFive HighFive_libdeps HighFive_HighFive # highfive libs
  nlohmann_json::nlohmann_json
  TBB::tbb
)
target_include_directories(core PRIVATE ${STB_INCLUDE_DIRS})
target_compile_definitions(core PUBLIC
  MET_WAVELENGTH_MIN=${MET_WAVELENGTH_MIN}
  MET_WAVELENGTH_MAX=${MET_WAVELENGTH_MAX}
  MET_WAVELENGTH_SAMPLES=${MET_WAVELENGTH_SAMPLES}
)

# Set up components library
add_library_r(components components)
target_link_libraries(components PUBLIC
  core
  small_gl
  imgui::imgui
  imguizmo::imguizmo
  unofficial::nativefiledialog::nfd
)
add_dependencies(components shaders_preprocess misc_copy)

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

# Set up playground executable
add_executable_r(playground src/target/main_playground.cpp)
target_link_libraries(playground PRIVATE core small_gl small_gl_parser_lib)

# Set up playground spv executable
add_executable_r(playground_spv src/target/main_playground_spv.cpp)
# set_target_properties(SPIRV-Tools-static PROPERTIES SUFFIX "")
target_link_libraries(playground_spv PRIVATE 
  core small_gl small_gl_parser_lib
  # glslang SPIRV OGLCompiler OSDependent

  # SPIRV-Tools-shared SPIRV-Tools-static SPIRV-Tools-opt
  # unofficial::shaderc_util::shaderc_util
  # unofficial::shaderc::shaderc
  spirv-cross-core
  spirv-cross-glsl
  spirv-cross-util
  spirv-cross-reflect
)

# Set up solve executable
add_executable_r(solve src/target/main_solve.cpp)
target_link_libraries(solve PRIVATE core small_gl small_gl_parser_lib)