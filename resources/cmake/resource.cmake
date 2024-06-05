cmake_minimum_required(VERSION 3.22)

function(add_resource_target target_name target_dir)
  # Generate list of relevant files
  file(GLOB_RECURSE misc_files ${CMAKE_CURRENT_SOURCE_DIR}/resources/${target_dir}/*)
  
  # Generate list of command functions to copy files
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

  # Specify custom target built on all command functions
  add_custom_target(${target_name} DEPENDS ${misc_copy_list})
endfunction()