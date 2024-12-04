#define STB_INCLUDE_IMPLEMENTATION
#define STB_INCLUDE_LINE_GLSL 

#include <array>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <stb_include.h>

/**
 * Application entry point; Expected parse arguments:
 * 1  : string filepath for input
 * 2  : string filepath for output
 * 3  : string filepath for include directory
 * 4+ : preprocessor arguments
 */
int main(int argc, char** argv) {
  if (argc <= 3) {
    std::cerr << "Failed; argc was: " 
             << argc 
             << ", but should be >= 3" 
             << std::endl;
    return EXIT_FAILURE;
  }

  char *fp_i = argv[1];
  char *fp_o = argv[2];

  int    fp_count = argc - 3;
  char** fp_incls = argv + 3;

  try {
    // Load file; test for success
    char error[256];
    char *data_load = stb_include_load_file(fp_i, nullptr);
    if (!data_load) {
      std::cerr << "Failed load; stb_include_file error was: " 
                << error 
                << std::endl;
      return EXIT_FAILURE;
    }

    // Preprocess file
    char *data_prep = stb_include_string(data_load, fp_incls, fp_count, fp_i, error);
    if (!data_prep) {
      std::cerr << "Failed preprocess; stb_include_string error was: " 
                << error 
                << std::endl;
      return EXIT_FAILURE;
    }

    // Save to output file
    std::ofstream ofs(fp_o, std::ios::out);
    if (!ofs.is_open()) {
      std::cerr << "Failed to open output file \"" 
                << fp_o
                << '\"' 
                << std::endl;
      return EXIT_FAILURE;
    }
    ofs.write(data_prep, strlen(data_prep));
    ofs.close();

    // Cleanup
    free(data_load);
    free(data_prep);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}