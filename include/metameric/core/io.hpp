#pragma once

#include <metameric/core/math.hpp>
#include <filesystem>
#include <string>

namespace met {
  namespace fs = std::filesystem;

  namespace io {
    // Return a copy of a provided path with a given extension (re)-placed
    inline
    fs::path path_with_ext(fs::path path, std::string_view ext) {
      return path.replace_extension(ext);
    }

    /* Data block for spectral texture export format */
    struct SpectralDataHeader {
      float wavelength_min    = MET_WAVELENGTH_MIN;
      float wavelength_max    = MET_WAVELENGTH_MAX;
      uint wavelength_samples = MET_WAVELENGTH_SAMPLES;
      uint function_count;
      uint weights_xres;
      uint weights_yres;
    };

    struct SpectralData {
      SpectralDataHeader header;
      std::vector<float> functions;
      std::vector<float> weights;
    };
    void save_spectral_data(const SpectralData &data, const fs::path &path);
    SpectralData load_spectral_data(const fs::path &path);

    /* Wrapper object for two-dimensional HD5 data files */
    struct HD5Data {
      std::vector<std::vector<float>> data;
      size_t size; // nr. of vectors
      size_t dims; // dimensionality of vectors
    };

    // HD5 file load from file
    HD5Data load_hd5(const fs::path &path, const std::string &name = "TotalRefs");

    // String load/save to/from file
    std::string load_string(const fs::path &path);
    void        save_string(const fs::path &path, const std::string &string);
  } // namespace io
} // namespace met