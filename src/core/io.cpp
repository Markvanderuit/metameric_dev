// Metameric includes
#include <metameric/core/io.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>

// Third party includes
#include <nlohmann/json.hpp>
#include <zstr.hpp>

// STL includes
#include <algorithm>
#include <execution>
#include <fstream>

namespace met::io {
  std::string load_string(const fs::path &path) {
    met_trace();

    // Check that file path exists
    debug::check_expr_dbg(fs::exists(path),
      fmt::format("failed to resolve path \"{}\"", path.string()));
      
    // Attempt to open file stream
    std::ifstream ifs(path, std::ios::ate);
    debug::check_expr_dbg(ifs.is_open(),
      fmt::format("failed to open file \"{}\"", path.string()));
      
    // Read file size and construct string to hold data
    size_t file_size = static_cast<size_t>(ifs.tellg());
    std::string str(file_size, ' ');

    // Set input position to start, then read full file into buffer
    ifs.seekg(0);
    ifs.read((char *) str.data(), file_size);
    ifs.close();

    return str;
  }

  void save_string(const fs::path &path, const std::string &str) {
    met_trace();
    
    // Attempt to open output file stream in text mode
    std::ofstream ofs(path, std::ios::out);
    debug::check_expr_dbg(ofs.is_open(),
      fmt::format("failed to open file \"{}\"", path.string()));

    // Write string directly to file in text mode
    ofs.write(str.data(), str.size());
    ofs.close();
  }

  Spec load_spec(const fs::path &path) {
    met_trace();

    // Output data blocks
    std::vector<float> wvls, values;

    // Read spectrum file as string, and parse line by line
    std::stringstream line__ss(load_string(path));
    std::string line;
    while (std::getline(line__ss, line)) {
      std::ranges::replace(line, '\t', ' ');
      auto split_line = line | std::views::split(' ');
      auto split_vect = std::vector<std::string>(range_iter(split_line));

      // Skip empty and commented lines
      guard_continue(!split_vect.empty() && split_vect[0][0] != '#');

      wvls.push_back(std::stof(split_vect[0]));
      values.push_back(std::stof(split_vect[1]));
    }

    return spectrum_from_data(wvls, values);
  }

  void save_spec(const fs::path &path, const Spec &s) {
    met_trace();

    // Get spectral data split into blocks
    auto [wvls, values] = spectrum_to_data(s);

    // Parse split data into string format
    std::stringstream ss;
    for (uint i = 0; i < wvls.size(); ++i)
      ss << fmt::format("{:.6f} {:.6f}\n", wvls[i], values[i]);

    return save_string(path, ss.str());
  }

  CMFS load_cmfs(const fs::path &path) {
    met_trace();

    // Output data blocks
    std::vector<float> wvls, values_x, values_y, values_z;

    // Read spectrum file as string, and parse line by line
    std::stringstream line__ss(load_string(path));
    std::string line;
    uint line_nr = 0;
    while (std::getline(line__ss, line)) {
      auto split_line = line | std::views::split(' ');
      auto split_vect = std::vector<std::string>(range_iter(split_line));

      // Skip empty and commented lines
      guard_continue(!split_vect.empty() && split_vect[0][0] != '#');

      // Throw on incorrect input data 
      debug::check_expr_rel(split_vect.size() == 4,
        fmt::format("CMFS data incorrect on line {}\n", line_nr));

      wvls.push_back(std::stof(split_vect[0]));
      values_x.push_back(std::stof(split_vect[1]));
      values_y.push_back(std::stof(split_vect[2]));
      values_z.push_back(std::stof(split_vect[3]));
      
      line_nr++;
    }

    return cmfs_from_data(wvls, values_x, values_y, values_z);
  }

  void save_cmfs(const fs::path &path, const CMFS &s) {
    met_trace();

    // Get spectral data split into blocks
    auto [wvls, values_x, values_y, values_z] = cmfs_to_data(s);

    // Parse split data into string format
    std::stringstream ss;
    for (uint i = 0; i < wvls.size(); ++i)
      ss << fmt::format("{:.6f} {:.6f} {:.6f} {:.6f}\n", wvls[i], values_x[i], values_y[i], values_z[i]);

    return save_string(path, ss.str());
  }

  Basis load_basis(const fs::path &path) {
    met_trace();

    // Output data blocks
    std::vector<float> wvls;
    std::vector<std::array<float, wavelength_bases>> values; 

    // Read spectrum file as string, and parse line by line
    std::stringstream line__ss(load_string(path));
    std::string line;
    uint line_nr = 0;
    while (std::getline(line__ss, line)) {
      auto split_line = line | std::views::split(' ');
      auto split_vect = std::vector<std::string>(range_iter(split_line));

      // Skip empty and commented lines
      guard_continue(!split_vect.empty() && split_vect[0][0] != '#');

      // Throw on incorrect input data 
      debug::check_expr_rel(split_vect.size() >= wavelength_bases + 1,
        fmt::format("Basis function data too short on line {}\n", line_nr));

      std::array<float, wavelength_bases> split_values;
      std::transform(split_vect.begin() + 1, 
                     split_vect.begin() + 1 + wavelength_bases, 
                     split_values.begin(), 
                     [](const auto &s) { return std::stof(s); });

      wvls.push_back(std::stof(split_vect[0]));
      values.push_back(split_values);

      line_nr++;
    }

    return basis_from_data(wvls, values);
  }

  void save_spectral_data(const SpectralData &data, const fs::path &path) {
    met_trace();
    
    // Attempt to open output file stream in binary mode using zlib stream wrapper
    zstr::ofstream ofs(path.string(), std::ios::out | std::ios::binary, 9);

    // Cast [0, 1]-bounded weights to 16 bit fixed point
    constexpr auto fcompact = [](float f) -> ushort { 
      constexpr float multiplier = static_cast<float>(std::numeric_limits<unsigned short>::max());
      return static_cast<ushort>(std::round(std::clamp(f, 0.f, 1.f) * multiplier)); 
    };
    std::vector<ushort> weights_compact(data.weights.size());
    std::transform(std::execution::par_unseq, range_iter(data.weights), weights_compact.begin(), fcompact);

    // Expected data sizes
    constexpr size_t header_size = sizeof(SpectralDataHeader);
    const size_t functions_size  = data.functions.size() * sizeof(decltype(data.functions)::value_type);
    const size_t weights_size    = data.weights.size() * sizeof(decltype(data.weights)::value_type);
    // const size_t weights_size    = weights_compact.size() * sizeof(decltype(weights_compact)::value_type);

    ofs.write((const char *) &data.header, header_size);
    ofs.write((const char *) data.functions.data(), functions_size);
    ofs.write((const char *) data.weights.data(), weights_size);
    // ofs.write((const char *) weights_compact.data(), weights_size);
    ofs.flush();
  }

  // Src: Mitsuba 0.5, reimplements InterpolatedSpectrum::average(...) from libcore/spectrum.cpp
  Spec spectrum_from_data(std::span<const float> wvls_, std::span<const float> values, bool remap) {
    met_trace();

    // Generate extended wavelengths for now, fitting current spectral range
    std::vector<float> wvls;
    if (remap) {
      wvls.resize(values.size());
      for (int i = 0; i < wvls.size(); ++i) {
        wvls[i] = wavelength_min + i * (wavelength_range / static_cast<float>(values.size() - 1));
      }
    } else {
      wvls = std::vector<float>(range_iter(wvls_));
    }

    Spec s = 0.f;

    float data_wvl_min = wvls[0],
          data_wvl_max = wvls[wvls.size() - 1];

    for (size_t i = 0; i < wavelength_samples; ++i) {
      float spec_wvl_min = wavelength_min + i * wavelength_ssize,
            spec_wvl_max = spec_wvl_min + wavelength_ssize;

      // Determine accessible range of wavelengths
      float wvl_min = std::max(spec_wvl_min, data_wvl_min),
            wvl_max = std::min(spec_wvl_max, data_wvl_max);
      guard_continue(wvl_max > wvl_min);

      // Find the starting index using binary search (Thanks for the idea, Mitsuba people!)
      ptrdiff_t pos = std::max(std::ranges::lower_bound(wvls, wvl_min) - wvls.begin(),
                              static_cast<ptrdiff_t>(1)) - 1;
      
      // Step through the provided data and integrate trapezoids
      for (; pos + 1 < wvls.size() && wvls[pos] < wvl_max; ++pos) {
        float wvl_a   = wvls[pos],
              value_a = values[pos],
              clamp_a = std::max(wvl_a, wvl_min);
        float wvl_b   = wvls[pos + 1],
              value_b = values[pos + 1],
              clamp_b = std::min(wvl_b, wvl_max);
        guard_continue(clamp_b > clamp_a);

        float inv_ab = 1.f / (wvl_b - wvl_a);
        float interp_a = std::lerp(value_a, value_b, (clamp_a - wvl_a) * inv_ab),
              interp_b = std::lerp(value_a, value_b, (clamp_b - wvl_a) * inv_ab);

        s[i] += .5f * (interp_a + interp_b) * (clamp_b - clamp_a);
      }
      s[i] /= wavelength_ssize;
    }

    return s.eval();
  }
 
  CMFS cmfs_from_data(std::span<const float> wvls, std::span<const float> values_x,
                      std::span<const float> values_y, std::span<const float> values_z) {
    met_trace();
    return (CMFS() << spectrum_from_data(wvls, values_x),
                      spectrum_from_data(wvls, values_y),
                      spectrum_from_data(wvls, values_z)).finished();
  }

  Basis basis_from_data(std::span<const float> wvls_, 
                        std::span<std::array<float, wavelength_bases>> values) {
    met_trace();

    // Generate extended wavelengths for now, fitting current spectral range
    std::vector<float> wvls(values.size() + 1);
    for (int i = 0; i < wvls.size(); ++i)
      wvls[i] = wavelength_min + i * (wavelength_range / static_cast<float>(values.size()));
  
    float data_wvl_min = wvls[0],
          data_wvl_max = wvls[wvls.size() - 1];

    Basis s = 0.f;

    for (size_t j = 0; j < wavelength_bases; ++j) {
      for (size_t i = 0; i < wavelength_samples; ++i) {
        float spec_wvl_min = i * wavelength_ssize + wavelength_min,
              spec_wvl_max = spec_wvl_min + wavelength_ssize;

        // Determine accessible range of wavelengths
        float wvl_min = std::max(spec_wvl_min, data_wvl_min),
              wvl_max = std::min(spec_wvl_max, data_wvl_max);
        guard_continue(wvl_max > wvl_min);

        // Find the starting index using binary search (Thanks for the idea, Mitsuba people!)
        ptrdiff_t pos = std::max(std::ranges::lower_bound(wvls, wvl_min) - wvls.begin(),
                                static_cast<ptrdiff_t>(1)) - 1;
        
        // Step through the provided data and integrate trapezoids
        for (; pos + 1 < wvls.size() && wvls[pos] < wvl_max; ++pos) {
          float wvl_a   = wvls[pos],
                value_a = values[pos][j],
                clamp_a = std::max(wvl_a, wvl_min);
          float wvl_b   = wvls[pos + 1],
                value_b = values[pos + 1][j],
                clamp_b = std::min(wvl_b, wvl_max);
          guard_continue(clamp_b > clamp_a);

          float inv_ab = 1.f / (wvl_b - wvl_a);
          float interp_a = std::lerp(value_a, value_b, (clamp_a - wvl_a) * inv_ab),
                interp_b = std::lerp(value_a, value_b, (clamp_b - wvl_a) * inv_ab);

          s(i, j) += .5f * (interp_a + interp_b) * (clamp_b - clamp_a);
        }
        s(i, j) /= wavelength_ssize;
      }
    }

    return s.eval();
  }
  
  std::array<std::vector<float>, 2> spectrum_to_data(const Spec &s) {
    std::vector<float> wvls(wavelength_samples);
    std::vector<float> values(wavelength_samples);

    std::ranges::transform(std::views::iota(0u, wavelength_samples), wvls.begin(), wavelength_at_index);
    std::ranges::copy(s, values.begin());

    return { wvls, values };
  }

  std::array<std::vector<float>, 4> cmfs_to_data(const CMFS &s)  {
    std::vector<float> wvls(wavelength_samples);
    std::vector<float> values_x(wavelength_samples), 
      values_y(wavelength_samples), values_z(wavelength_samples);
    
    std::ranges::transform(std::views::iota(0u, wavelength_samples), wvls.begin(), wavelength_at_index);
    std::ranges::copy(s.col(0), values_x.begin());
    std::ranges::copy(s.col(1), values_y.begin());
    std::ranges::copy(s.col(2), values_z.begin());
    
    return { wvls, values_x, values_y, values_z };
  }
} // namespace met::io