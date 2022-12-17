// Metameric includes
#include <metameric/core/io.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>

// Third party includes
#include <highfive/H5File.hpp>
#include <nlohmann/json.hpp>

// STL includes
#include <algorithm>
#include <execution>
#include <fstream>

namespace met::io {
  namespace detail {
    template <typename T>
    constexpr inline
    std::vector<std::vector<T>> transpose(const std::vector<std::vector<T>> &v) {
      met_trace();

      std::vector<std::vector<T>> wr(v[0].size(), std::vector<T>(v.size()));

      #pragma omp parallel for // target seq. writes and less thread spawns
      // TODO sequence out for TBB
      for (int i = 0; i < static_cast<int>(v[0].size()); ++i) {
        auto &wri = wr[i];
        for (size_t j = 0; j < v.size(); ++j) {
          wri[j] = v[j][i];
        }
      }

      return wr;
    }
  } // namespace detail

  HD5Data load_hd5(const fs::path &path, const std::string &name) {
    met_trace();

    // Check that file path exists
    debug::check_expr_dbg(fs::exists(path),
      fmt::format("failed to resolve path \"{}\"", path.string()));

    // Attempt to open file and extract forcibly named dataset from file
    HighFive::File file(path.string(), HighFive::File::ReadOnly);
    HighFive::DataSet ds = file.getDataSet(name);

    // Read file properties into data object
    HD5Data obj;
    ds.read(obj.data);
    obj.data = detail::transpose(obj.data);
    obj.size = obj.data.size();
    obj.dims = obj.data[0].size();

    return obj;
  }

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
      ss << fmt::format("{} {}\n", wvls[i], values[i]);

    return save_string(path, ss.str());
  }

  CMFS load_cmfs(const fs::path &path) {
    met_trace();

    // Output data blocks
    std::vector<float> wvls, values_x, values_y, values_z;

    // Read spectrum file as string, and parse line by line
    std::stringstream line__ss(load_string(path));
    std::string line;
    while (std::getline(line__ss, line)) {
      auto split_line = line | std::views::split(' ');
      auto split_vect = std::vector<std::string>(range_iter(split_line));

      // Skip empty and commented lines
      guard_continue(!split_vect.empty() && split_vect[0][0] != '#');

      wvls.push_back(std::stof(split_vect[0]));
      values_x.push_back(std::stof(split_vect[1]));
      values_y.push_back(std::stof(split_vect[2]));
      values_z.push_back(std::stof(split_vect[3]));
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
      ss << fmt::format("{} {} {} {}\n", wvls[i], values_x[i], values_y[i], values_z[i]);

    return save_string(path, ss.str());
  }

  void save_spectral_data(const SpectralData &data, const fs::path &path) {
    met_trace();
    
    // Attempt to open output file stream in binary mode
    std::ofstream ofs(path, std::ios::out | std::ios::binary);
    debug::check_expr_dbg(ofs.is_open(),
      fmt::format("failed to open file \"{}\"", path.string()));

    // Expected data sizes
    constexpr size_t header_size = sizeof(SpectralDataHeader);
    const size_t functions_size = data.functions.size() * sizeof(decltype(data.functions)::value_type);
    const size_t weights_size = data.weights.size() * sizeof(decltype(data.weights)::value_type);

    // Write data in three steps
    ofs.write((const char *) &data.header, header_size);
    ofs.write((const char *) data.functions.data(), functions_size);
    ofs.write((const char *) data.weights.data(), weights_size);

    ofs.close();
  }

  // Src: Mitsuba 0.5, reimplements InterpolatedSpectrum::eval(...) from libcore/spectrum.cpp
  Spec spectrum_from_data(std::span<const float> wvls, std::span<const float> values) {
    met_trace();

    float data_wvl_min = wvls[0],
          data_wvl_max = wvls[wvls.size() - 1];

    Spec s = 0.f;
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
  
  std::array<std::vector<float>, 2> spectrum_to_data(const Spec &s) {
    std::vector<float> wvls(wavelength_samples);
    std::vector<float> values(wavelength_samples);

    std::ranges::transform(std::views::iota(0u, wavelength_samples), wvls.begin(), wavelength_at_index);
    std::ranges::copy(s, values.begin());

    return { wvls, values };
  }

  std::array<std::vector<float>, 4> cmfs_to_data(const CMFS &s) {
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