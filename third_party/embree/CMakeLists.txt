cmake_minimum_required(VERSION 3.22)

include(dependency)

# Specify import libraries for embree/tbb shared data
add_library(embree SHARED IMPORTED GLOBAL)
add_library(tbb    SHARED IMPORTED GLOBAL)

# Per platform, specify embree includes and embree/tbb shared object libraries
if(WIN32)
  # Setup embree/tbb imports
  target_include_directories(embree INTERFACE windows/include)
  set_target_properties(embree 
    PROPERTIES 
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/windows/bin/embree4.dll
    IMPORTED_IMPLIB   ${CMAKE_CURRENT_SOURCE_DIR}/windows/lib/embree4.lib
  )
  set_target_properties(tbb 
    PROPERTIES 
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/windows/bin/tbb12.dll
    IMPORTED_IMPLIB   ${CMAKE_CURRENT_SOURCE_DIR}/windows/lib/tbb12.lib
  )
  target_link_libraries(embree INTERFACE tbb)

  # Copy files to bin directory; a bit hacky, but runtime dependencies are confusing
  add_file_copy_target(
    embree_copy ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/bin/embree4.dll;${CMAKE_CURRENT_SOURCE_DIR}/windows/bin/tbb12.dll"
  )
elseif(UNIX)
  # Setup embree/tbb imports
  target_include_directories(embree INTERFACE linux/include)
  set_target_properties(embree 
    PROPERTIES 
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/linux/lib/libembree4.so.4
  )
  set_target_properties(tbb 
    PROPERTIES 
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/linux/lib/libtbb.so.12
  )
  target_link_options(tbb      INTERFACE "LINKER:-rpath,./lib/")
  target_link_options(embree   INTERFACE "LINKER:-rpath,./lib/")
  target_link_libraries(embree INTERFACE tbb)
  
  # Copy files to bin/lib directory; a bit hacky, but runtime dependencies are confusing
  add_file_copy_target(
    embree_copy ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/lib
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/lib/libembree4.so.4;${CMAKE_CURRENT_SOURCE_DIR}/linux/lib/libtbb.so.12"
  )
else()
  message(FATAL_ERROR "No Intel Embree binaries included for this platform")
endif()