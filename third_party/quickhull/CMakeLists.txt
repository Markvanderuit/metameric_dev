cmake_minimum_required(VERSION 3.22)

add_library(quickhull src/quickhull.cpp)
target_include_directories(quickhull PUBLIC include)
target_compile_features(quickhull PUBLIC cxx_std_14)