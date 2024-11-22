cmake_minimum_required(VERSION 3.22)

function(met_add_folder_copy_target target_name input_dir output_dir)
  # Specify target with attached commands
  add_custom_target(
    ${target_name}
    
    COMMENT "Directory copy: from ${input_dir} to ${output_dir}"

    # Ensure target directory exists
    COMMAND ${CMAKE_COMMAND}
            -E make_directory ${output_dir}

    # Perform directory copy
    COMMAND ${CMAKE_COMMAND} 
            -E copy_directory ${input_dir} ${output_dir}
  )
endfunction()

function(met_add_file_copy_target target_name output_dir inputs)
  # Iterate inputs
  foreach(input ${inputs})
    # Build target path
    cmake_path(GET input FILENAME filename)
    set(output "${output_dir}/${filename}")

    # Register copy command
    add_custom_command(
      COMMENT "File copy: from ${input} to ${output}"
      OUTPUT  ${output}
      COMMAND ${CMAKE_COMMAND} -E copy ${input} ${output}
      DEPENDS ${input}
    )
    list(APPEND file_copy_list ${output})
  endforeach()

  # Bind commands together in single target
  add_custom_target(${target_name} DEPENDS ${file_copy_list})
endfunction()