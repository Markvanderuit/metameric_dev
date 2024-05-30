vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO RenderKit/embree
    REF v4.3.1
    SHA512 da7710c6dfaa90970c223a503702fc7c7dd86c1397372b3d6f51c4377d28d8e62b90ee8c99b70e3aa49e16971a5789bb8f588ea924881b9dd5dd8d5fcd16518a
    HEAD_REF master
)

string(COMPARE EQUAL ${VCPKG_LIBRARY_LINKAGE} static EMBREE_STATIC_LIB)
string(COMPARE EQUAL ${VCPKG_CRT_LINKAGE} static EMBREE_STATIC_RUNTIME)

# Automatically select best ISA based on platform or VCPKG_CMAKE_CONFIGURE_OPTIONS.
vcpkg_list(SET EXTRA_OPTIONS)
if(VCPKG_TARGET_IS_EMSCRIPTEN)
    # Disable incorrect ISA set for Emscripten and enable NEON which is supported and should provide decent performance.
    # cf. [Using SIMD with WebAssembly](https://emscripten.org/docs/porting/simd.html#using-simd-with-webassembly)
    vcpkg_list(APPEND EXTRA_OPTIONS
        -DEMBREE_MAX_ISA:STRING=NONE
        -DEMBREE_ISA_AVX:BOOL=OFF
        -DEMBREE_ISA_AVX2:BOOL=OFF
        -DEMBREE_ISA_AVX512:BOOL=OFF
        -DEMBREE_ISA_SSE2:BOOL=OFF
        -DEMBREE_ISA_SSE42:BOOL=OFF
        -DEMBREE_ISA_NEON:BOOL=ON
    )
elseif(VCPKG_TARGET_IS_OSX AND (VCPKG_TARGET_ARCHITECTURE STREQUAL "arm64"))
    # The best ISA for Apple arm64 is unique and unambiguous.
    vcpkg_list(APPEND EXTRA_OPTIONS
        -DEMBREE_MAX_ISA:STRING=NONE
    )
elseif(VCPKG_TARGET_IS_OSX AND (VCPKG_TARGET_ARCHITECTURE STREQUAL "x64") AND (VCPKG_LIBRARY_LINKAGE STREQUAL "static"))
    # AppleClang >= 9.0 does not support selecting multiple ISAs.
    # Let Embree select the best and unique one.
    vcpkg_list(APPEND EXTRA_OPTIONS
        -DEMBREE_MAX_ISA:STRING=DEFAULT
    )
else()
    # Let Embree select the best ISA set for the targeted platform.
    vcpkg_list(APPEND EXTRA_OPTIONS
        -DEMBREE_MAX_ISA:STRING=NONE
    )
endif()

set(EMBREE_TASKING_SYSTEM "TBB")

vcpkg_replace_string("${SOURCE_PATH}/common/cmake/installTBB.cmake" "IF (EMBREE_STATIC_LIB)" "IF (0)")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    DISABLE_PARALLEL_CONFIGURE
    OPTIONS
      ${EXTRA_OPTIONS}
      -DBUILD_TESTING=OFF
      -DEMBREE_TESTING_INSTALL_TESTS=OFF
      -DEMBREE_ZIP_MODE=OFF
      -DEMBREE_GEOMETRY_QUAD=OFF
      -DEMBREE_GEOMETRY_GRID=OFF
      -DEMBREE_GEOMETRY_POINT=OFF
      -DEMBREE_GEOMETRY_SUBDIVISION=OFF
      -DEMBREE_GEOMETRY_CURVE=OFF
      -DEMBREE_GEOMETRY_INSTANCE=OFF
      -DEMBREE_GEOMETRY_USER=OFF
      -DEMBREE_IGNORE_INVALID_RAYS=OFF
      -DEMBREE_RAY_MASK=OFF
      -DEMBREE_STAT_COUNTERS=OFF
      -DEMBREE_ISPC_SUPPORT=OFF
      -DEMBREE_TUTORIALS=OFF
      -DEMBREE_STATIC_RUNTIME=${EMBREE_STATIC_RUNTIME}
      -DEMBREE_STATIC_LIB=${EMBREE_STATIC_LIB}
      -DEMBREE_TASKING_SYSTEM:STRING=${EMBREE_TASKING_SYSTEM}
      -DEMBREE_INSTALL_DEPENDENCIES=OFF
      MAYBE_UNUSED_VARIABLES
      EMBREE_STATIC_RUNTIME
)


vcpkg_cmake_install()
vcpkg_copy_pdbs()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/embree-4.3.1 PACKAGE_NAME embree)
set(config_file "${CURRENT_PACKAGES_DIR}/share/embree/embree-config.cmake")
# Fix details in config.
file(READ "${config_file}" contents)
string(REPLACE "SET(EMBREE_BUILD_TYPE Release)" "" contents "${contents}")
string(REPLACE "/../../../" "/../../" contents "${contents}")
string(REPLACE "FIND_PACKAGE" "include(CMakeFindDependencyMacro)\n  find_dependency" contents "${contents}")
string(REPLACE "REQUIRED" "COMPONENTS" contents "${contents}")
string(REPLACE "/lib/cmake/embree-4.3.1" "/share/embree" contents "${contents}")

if(NOT VCPKG_BUILD_TYPE)
    string(REPLACE "/lib/embree4.lib" "$<$<CONFIG:DEBUG>:/debug>/lib/embree4.lib" contents "${contents}")
endif()
file(WRITE "${config_file}" "${contents}")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/bin" "${CURRENT_PACKAGES_DIR}/debug/bin")
endif()
if(APPLE)
    file(REMOVE "${CURRENT_PACKAGES_DIR}/uninstall.command" "${CURRENT_PACKAGES_DIR}/debug/uninstall.command")
endif()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")