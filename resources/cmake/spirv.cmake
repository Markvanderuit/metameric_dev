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

function(compile_glsl_to_spirv glsl_src_fp spirv_dependencies)
  # Obtain path, stripped of lead and filename
  cmake_path(RELATIVE_PATH glsl_src_fp
             BASE_DIRECTORY ${PROJECT_SOURCE_DIR}/resources/shaders/src
             OUTPUT_VARIABLE glsl_rel_fp)

  message(STATUS "Found shader : ${glsl_rel_fp}")

  set(spirv_parse_fp "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/${glsl_rel_fp}")
  set(spirv_temp_fp  "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/${glsl_rel_fp}.temp")
  set(spirv_bin_fp   "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/${glsl_rel_fp}.spv")
  set(spirv_refl_fp  "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/${glsl_rel_fp}.json")

  # Add preprocessor stage to handle #include as well as > c98 preprocessing
  # we reuse the local c++ compiler to avoid google_include extensions and
  # allow for varargs
  # TODO; adapt dependent on compiler
  add_custom_command(
    OUTPUT  ${spirv_parse_fp}
    COMMAND ${CMAKE_CXX_COMPILER}
            # Primary arguments
            -Tp                                                  # treat this as a c++ source file
            ${glsl_src_fp}                                       # source file
            -EP                                                  # override line directive
            -P                                                   # preprocess to file
            -C                                                   # preserve comments
            -I "${PROJECT_SOURCE_DIR}/resources/shaders/include" # include directory
            -Zc:preprocessor                                     # make nice recent c++ macro expansions available
            -std:c++20                                           # make nice recent c++ macro expansions available
            -Fi${spirv_parse_fp}                                 # preprocess file output name

            # Define arguments
            ${preprocessor_defines}
    DEPENDS ${glsl_src_fp} ${glsl_includes}
    VERBATIM
  )
  
  # Generate spir-v binary using glslangValidator, store in .temp file
  add_custom_command(
    OUTPUT  ${spirv_temp_fp}
    COMMAND ${glslangValidator} ${spirv_parse_fp} 
            -o ${spirv_temp_fp} 
            --client opengl100 
            --target-env spirv1.5 
            --glsl-version 460
    DEPENDS ${spirv_parse_fp}
    VERBATIM
  )

  # Generate optimized spir-v binary using spirv-opt from spirv-tools
  add_custom_command(
    OUTPUT  ${spirv_bin_fp}
    COMMAND ${spirv-opt} ${spirv_temp_fp} 
            -o ${spirv_bin_fp} 
            -O 
            -Os
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