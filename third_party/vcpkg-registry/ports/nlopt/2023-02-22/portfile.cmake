vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO stevengj/nlopt
    REF 3f9cfd352ad60083df26aeffe85f3ca277e26887
    SHA512 
    835ffff2d3e4cf6b863a7d26947c678a69bac32b43292e1d1b5570e485a1743ae6ec2ebec97a7f3da1650844dc8f0c9177c483b0c8d727e50a76ba6a56215666
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DNLOPT_FORTRAN=OFF
        -DNLOPT_PYTHON=OFF
        -DNLOPT_OCTAVE=OFF
        -DNLOPT_MATLAB=OFF
        -DNLOPT_GUILE=OFF
        -DNLOPT_SWIG=OFF
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/nlopt)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

file(INSTALL "${SOURCE_PATH}/COPYING" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)

vcpkg_fixup_pkgconfig()
