cmake_minimum_required(VERSION 3.22)

# Find included third party shader-related tools
if (WIN32)
  set(bin_tools_path ${CMAKE_CURRENT_LIST_DIR}/bin/windows)
elseif(UNIX)
  set(bin_tools_path ${CMAKE_CURRENT_LIST_DIR}/bin/linux)
else()
  message(FATAL_ERROR "SPIR-V tools do not support this platform" )
endif()
find_program(glslangValidator NAMES glslangValidator PATHS ${bin_tools_path} NO_DEFAULT_PATH REQUIRED)
find_program(spirv-opt        NAMES spirv-opt        PATHS ${bin_tools_path} NO_DEFAULT_PATH REQUIRED)
find_program(spirv-cross      NAMES spirv-cross      PATHS ${bin_tools_path} NO_DEFAULT_PATH REQUIRED)

# Ensure the target stb_include_app is available to handle glsl includes with less
# finicking than GL_ARB_shading_language_include
add_executable(stb_include_app                     ${CMAKE_SOURCE_DIR}/resources/cmake/stb_include/stb_include_app.cpp)
target_include_directories(stb_include_app PRIVATE ${CMAKE_SOURCE_DIR}/resources/cmake/stb_include/include)

# Generate preamble file; we avoid passing preprocessor defines using
# some precompile step or glslangValidator, as we sometimes don't want
# to use the spirv pipeline (like on a certain presentation demo on my
# crappy integrated graphics). Instead, we embed a conf file with defines
# in preamble.glsl, which the shader includes will be able to find.
set(met_preamble_file ${CMAKE_BINARY_DIR}/preamble.glsl)
FILE(WRITE ${met_preamble_file} "#version 460 core
#define MET_WAVELENGTH_MIN        ${MET_WAVELENGTH_MIN} 
#define MET_WAVELENGTH_MAX        ${MET_WAVELENGTH_MAX} 
#define MET_WAVELENGTH_SAMPLES    ${MET_WAVELENGTH_SAMPLES}
#define MET_WAVELENGTH_BASES      ${MET_WAVELENGTH_BASES} 
#define MET_SUPPORTED_MESHES      ${MET_SUPPORTED_MESHES}
#define MET_SUPPORTED_OBJECTS     ${MET_SUPPORTED_OBJECTS}
#define MET_SUPPORTED_EMITTERS    ${MET_SUPPORTED_EMITTERS}
#define MET_SUPPORTED_CONSTRAINTS ${MET_SUPPORTED_CONSTRAINTS}
#define MET_SUPPORTED_TEXTURES    ${MET_SUPPORTED_TEXTURES}
")

function(compile_glsl_to_spirv input_dir output_dir glsl_path output_dependencies)
  # Obtain filepath, stripped of relative lead (/shaders/render/shader.glsl)
  cmake_path(RELATIVE_PATH glsl_path
             BASE_DIRECTORY ${PROJECT_SOURCE_DIR}/resources/shaders/src
             OUTPUT_VARIABLE glsl_path_stripped)

  # Obtain directory, stripped of lead (/shaders/render/)
  cmake_path(GET glsl_path_stripped PARENT_PATH glsl_parent_path)

  # Obtain filename, stripped of extension (shader.glsl)
  cmake_path(GET glsl_path_stripped FILENAME glsl_filename)

  # Output path shorthands
  set(spv_directory   "${output_dir}/${glsl_parent_path}/")     # Working directory for output
  set(spv_path_parse  "${spv_directory}/${glsl_filename}")      # Temporary path for stb_include parsed output
  set(spv_path_binary "${spv_directory}/${glsl_filename}.spv")  # Path to generated .spv binary
  set(spv_path_json   "${spv_directory}/${glsl_filename}.json") # Path to generated .json reflect information

  # Add preprocessor stage to handle #include as well as > c98 preprocessing
  # we reuse the local c++ compiler to avoid google_include extensions and
  # allow for varargs
  add_custom_command(
    # Expected output files are .spv, .json
    OUTPUT ${spv_path_binary} ${spv_path_json}

    # Beforehand; ensure output directory exists
    COMMAND ${CMAKE_COMMAND} -E make_directory ${spv_directory}

    # First command; parse includes; we avoid use of GL_ARB_shading_language_include as it
    # and glslangvalidator seem to be... finicky?
    COMMAND stb_include_app ${glsl_path} ${spv_path_parse} "${input_dir}/include" "${CMAKE_BINARY_DIR}"
    
    # Second command; nuke shader cache if one currently exists
    COMMAND ${CMAKE_COMMAND} -E rm -f "${output_dir}/shaders.bin"

    # Third command; generate spirv binary using glslangvalidator
    COMMAND ${glslangValidator} 
            ${spv_path_parse}       # input glsl
            -o ${spv_path_binary}   # output binary
            --client opengl100      # create binary under OpenGL semantics
            --target-env spirv1.5   # execution environment is spirv 1.5

    # Fourth command; generate reflection information in .json files using spirv-cross
    COMMAND ${spirv-cross} ${spv_path_binary} --output ${spv_path_json} --reflect

    # # Fifth command; remove parsed glsl file
    # COMMAND ${CMAKE_COMMAND} -E rm -f ${spv_path_parse}

    DEPENDS ${glsl_path} ${glsl_includes} stb_include_app
    VERBATIM
  )

  # Append output files to list of tracked outputs
  list(APPEND output_dependencies ${spv_path_binary} ${spv_path_json})

  # Propagate built output files to list of tracked outputs
  set(output_dependencies "${output_dependencies}" PARENT_SCOPE)
endfunction()

function(compile_glsl_to_spirv_list input_dir output_dir glsl_srcs_fp output_dependencies)
  foreach(glsl_src_fp ${glsl_srcs_fp})
    compile_glsl_to_spirv(${input_dir} ${output_dir} ${glsl_src_fp} "${output_dependencies}")
  endforeach()

  # Propagate built output files to list of tracked outputs
  set(output_dependencies "${output_dependencies}" PARENT_SCOPE)
endfunction()

function(met_add_shader_target target_name input_dir output_dir)
  # Glob all shader files
  file(GLOB_RECURSE glsl_sources
    ${input_dir}/src/*.frag
    ${input_dir}/src/*.geom
    ${input_dir}/src/*.vert
    ${input_dir}/src/*.comp
  )
  file(GLOB_RECURSE glsl_includes 
    ${input_dir}/include/*
  )

  # Initial list of tracked files is the include headers
  set(output_dependencies ${glsl_includes})
  
  # Generate compilation path
  compile_glsl_to_spirv_list(${input_dir} ${output_dir} "${glsl_sources}" "${output_dependencies}")

  # Specify custom target built on all tracked files
  add_custom_target(${target_name} DEPENDS ${output_dependencies})
endfunction()