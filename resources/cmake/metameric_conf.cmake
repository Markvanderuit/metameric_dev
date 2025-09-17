cmake_minimum_required(VERSION 3.22)

# Spectral layout; inserted in code/shaders as preprocessor defines
set(MET_WAVELENGTH_MIN     380) # Minimum wavelength of spectral range
set(MET_WAVELENGTH_MAX     780) # Maximum wavelength of spectral range
set(MET_WAVELENGTH_SAMPLES  64) # Nr. of bins used in discrete spectral representations
set(MET_WAVELENGTH_BASES    12) # Maximum nr. of bases used in generative functions

# Some object limits for renderer; inserted in code/shaders as preprocessor defines
set(MET_SUPPORTED_MESHES        32) # Maximum supported scene meshes
set(MET_SUPPORTED_OBJECTS       64) # Maximum supported scene objects
set(MET_SUPPORTED_EMITTERS      32) # Maximum supported scene emitters
set(MET_SUPPORTED_TEXTURES     128) # Maximum supported scene textures
set(MET_SUPPORTED_CONSTRAINTS  512) # Maximum supported boundary + interior spectra per uplifting

# Print configuration info
message(STATUS "Metameric : Enabling exceptions = ${MET_ENABLE_EXCEPTIONS}")
message(STATUS "Metameric : Enabling Tracy      = ${MET_ENABLE_TRACY}")
message(STATUS "Metameric : Wavelength min.     = ${MET_WAVELENGTH_MIN}")
message(STATUS "Metameric : Wavelength max.     = ${MET_WAVELENGTH_MAX}")
message(STATUS "Metameric : Wavelength samples  = ${MET_WAVELENGTH_SAMPLES}")
message(STATUS "Metameric : Wavelength bases    = ${MET_WAVELENGTH_BASES}")

# Embed the above in a preprocessor define list
set(MET_PREPROCESSOR_DEFINES
  -DMET_WAVELENGTH_MIN=${MET_WAVELENGTH_MIN} 
  -DMET_WAVELENGTH_MAX=${MET_WAVELENGTH_MAX} 
  -DMET_WAVELENGTH_SAMPLES=${MET_WAVELENGTH_SAMPLES} 
  -DMET_WAVELENGTH_BASES=${MET_WAVELENGTH_BASES} 
  -DMET_SUPPORTED_MESHES=${MET_SUPPORTED_MESHES}
  -DMET_SUPPORTED_OBJECTS=${MET_SUPPORTED_OBJECTS}
  -DMET_SUPPORTED_EMITTERS=${MET_SUPPORTED_EMITTERS}
  -DMET_SUPPORTED_CONSTRAINTS=${MET_SUPPORTED_CONSTRAINTS}
  -DMET_SUPPORTED_TEXTURES=${MET_SUPPORTED_TEXTURES}
)

# Pass editor options into list
if(MET_ENABLE_EXCEPTIONS)
  list(APPEND MET_PREPROCESSOR_DEFINES -DMET_ENABLE_EXCEPTIONS)
endif()
if(MET_ENABLE_TRACY)
  list(APPEND MET_PREPROCESSOR_DEFINES -DMET_ENABLE_TRACY)
endif()

# Helper function to add a library target
function(met_add_library target_name include_names)
  # Gather source files recursively
  file(GLOB_RECURSE src_files ${CMAKE_CURRENT_SOURCE_DIR}/src/${target_name}/*.cpp)
  
  # Specify target library
  add_library(${target_name} ${src_files})

  # Specify target includes/features/defines
  target_include_directories(${target_name} PUBLIC include ${include_names})
  target_compile_features(${target_name}    PUBLIC cxx_std_23)
  target_compile_definitions(${target_name} PUBLIC _USE_MATH_DEFINES ${MET_PREPROCESSOR_DEFINES})

  # Enable /bigobj to make MSVC stop complaining about its restricted object sizes
  if (MSVC)
    target_compile_options(${target_name} PRIVATE /bigobj)
  endif()
endfunction()

function(met_add_pch target_name)
  file(GLOB_RECURSE header_files ${CMAKE_CURRENT_SOURCE_DIR}/include/${target_name}/*hpp)
  target_precompile_headers(${target_name} PUBLIC ${header_files})
endfunction()

function(met_reuse_pch target_name other_names)
  foreach(other_name ${other_names})
    target_precompile_headers(${target_name} REUSE_FROM ${other_name})
  endforeach()
endfunction()