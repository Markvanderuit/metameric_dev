#pragma once

#include <metameric/core/math.hpp>
#include <filesystem>
#include <string>

namespace met::io {
  /* Wrapper object for two-dimensional HD5 data files */
  struct HD5Data {
    std::vector<std::vector<float>> data;
    size_t size; // nr. of vectors
    size_t dims; // dimensionality of vectors
  };

  // HD5 file load from file
  HD5Data load_hd5(const std::filesystem::path &path, const std::string &name = "TotalRefs");

  // String load/save to/from file
  std::string load_string(const std::filesystem::path &path);
  void        save_string(const std::filesystem::path &path, const std::string &string);
} // namespace met::io