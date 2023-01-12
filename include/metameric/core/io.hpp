#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
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

    /* Header block for spectral texture export format */
    struct SpectralDataHeader {
      float wvl_min    = wavelength_min;
      float wvl_max    = wavelength_max;
      uint wvl_samples = wavelength_samples;
      uint func_count;
      uint wght_xres;
      uint wght_yres;
    };
    /* Data block for spectral texture export format */
    struct SpectralData {
      SpectralDataHeader header;
      std::span<float> functions;
      std::span<float> weights;
    };
    void save_spectral_data(const SpectralData &data, const fs::path &path);

    /* Wrapper object for two-dimensional HD5 data files */
    struct HD5Data {
      std::vector<std::vector<float>> data;
      size_t size; // nr. of vectors
      size_t dims; // dimensionality of vectors
    };

    // HD5 file load from file
    HD5Data load_hd5(const fs::path &path, const std::string &name = "TotalRefs");

    // Simple string load/save to/from file
    std::string load_string(const fs::path &path);
    void        save_string(const fs::path &path, const std::string &string);

    // Simple spectrum load/save to/from file
    // Input should be a text file, containing a single wavelength and measured value per line, and
    // optional comments marked with '#'. This is the same format used in the Mitsuba renderer
    Spec load_spec(const fs::path &path);
    void save_spec(const fs::path &path, const Spec &s);

    // Simple cmfs load/save to/from file
    // Input should be a text file, containing a single wavelength and three values per line, and
    // optional comments marked with '#'. This is similar to the spectrum format described above.
    CMFS load_cmfs(const fs::path &path);
    void save_cmfs(const fs::path &path, const CMFS &s);

    // Simple basis function load from file
    // Input should be a text file, containing a single wavelength and 'm' values per line, and
    // optional comments marked with '#'. This is similar to the spectrum format described above.
    // See CmakeLists.txt, 'MET_WAVELENGTH_BASES' for the value of 'm'.
    Basis load_basis(const fs::path &path);
    

    // Load a discrete spectral distribution from sequentially increasing wvl/value data
    Spec spectrum_from_data(std::span<const float> wvls, std::span<const float> values);

    // Load a discrete trio of color matching functions from sequentially increasing wvl/value data
    CMFS cmfs_from_data(std::span<const float> wvls,  std::span<const float> values_x,
                        std::span<const float> values_y, std::span<const float> values_z);

    // Load a set of basis functions from sequentially increasing wvl/value data
    Basis basis_from_data(std::span<const float> wvls, 
                          std::span<std::array<float, wavelength_bases>> values);

    // Split a discrete spectral distribution or color matching functions into sequentially 
    // increasing wvl/value/*/* data
    std::array<std::vector<float>, 2> spectrum_to_data(const Spec &s);
    std::array<std::vector<float>, 4> cmfs_to_data(const CMFS &s);
  } // namespace io
} // namespace met