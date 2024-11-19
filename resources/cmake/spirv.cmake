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

function(compile_glsl_to_spirv glsl_src_fp spirv_dependencies)
  # Obtain path, stripped of lead
  cmake_path(RELATIVE_PATH glsl_src_fp
             BASE_DIRECTORY ${PROJECT_SOURCE_DIR}/resources/shaders/src
             OUTPUT_VARIABLE glsl_rel_fp)

  # Obtain directory, stripped of filename
  cmake_path(GET glsl_rel_fp PARENT_PATH glsl_rel_dp)

  # Path shorthands
  set(spirv_parse_fp "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${glsl_rel_fp}")
  set(spirv_temp_fp  "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${glsl_rel_fp}.temp")
  set(spirv_bin_fp   "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${glsl_rel_fp}.spv")
  set(spirv_refl_fp  "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${glsl_rel_fp}.json")

  # Add preprocessor stage to handle #include as well as > c98 preprocessing
  # we reuse the local c++ compiler to avoid google_include extensions and
  # allow for varargs
  add_custom_command(
    OUTPUT  ${spirv_bin_fp} ${spirv_refl_fp}

    # Beforehand; ensure output/working directory exists
    COMMAND ${CMAKE_COMMAND}
            -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${glsl_rel_dp}"

    # First command; parse includes; we avoid use of GL_ARB_shading_language_include as it
    # and glslangvalidator seem to be... finicky?
    COMMAND stb_include_app
            ${glsl_src_fp}
            "${PROJECT_SOURCE_DIR}/resources/shaders/include"
            ${spirv_parse_fp}
    
    # Second command; nuke shader cache if one currently exists
    COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/shaders.bin"

    # Third command; generate spirv binary using glslangvalidator
    COMMAND ${glslangValidator} 
            ${spirv_parse_fp}       # input glsl
            -o ${spirv_bin_fp}      # output binary
            --client opengl100      # create binary under OpenGL semantics
            --target-env spirv1.5   # execution environment is spirv 1.5
            ${preprocessor_defines} # forward -D... arguments

    # Fourth command; generate reflection information in .json files using spirv-cross
    COMMAND ${spirv-cross} 
            ${spirv_bin_fp} 
            --output ${spirv_refl_fp} 
            --reflect

    # Fifth command; remove parsed glsl file
    COMMAND ${CMAKE_COMMAND} -E rm -f ${spirv_parse_fp}

    DEPENDS ${glsl_src_fp} ${glsl_includes} stb_include_app
    VERBATIM
  )

  # Append output files to list of tracked outputs
  list(APPEND spirv_dependencies ${spirv_bin_fp} ${spirv_refl_fp})
  set(spirv_dependencies "${spirv_dependencies}" PARENT_SCOPE)
endfunction()

function(compile_glsl_to_spirv_list glsl_srcs_fp spirv_dependencies)
  foreach(glsl_src_fp ${glsl_srcs_fp})
    compile_glsl_to_spirv(${glsl_src_fp} "${spirv_dependencies}")
  endforeach()
  set(spirv_dependencies "${spirv_dependencies}" PARENT_SCOPE)
endfunction()

function(add_shader_target target_name cache_name)
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
  
  # Generate list of command functions
  set(spirv_dependencies ${glsl_includes})
  compile_glsl_to_spirv_list("${glsl_srcs}" "${spirv_dependencies}")

  # Specify custom target built on all command functions
  add_custom_target(${target_name} DEPENDS ${spirv_dependencies})
endfunction()