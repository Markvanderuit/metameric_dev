// Metameric includes
#include <metameric/core/io.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>

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
      std::vector<std::vector<T>> wr(v[0].size(), std::vector<T>(v.size()));

      #pragma omp parallel for // target seq. writes and less thread spawns
      for (size_t i = 0; i < v[0].size(); ++i) {
        auto &wri = wr[i];
        for (size_t j = 0; j < v.size(); ++j) {
          wri[j] = v[j][i];
        }
      }

      return wr;
    }
  } // namespace detail

  HD5Data load_hd5(const fs::path &path, const std::string &name) {
    // Check that file path exists
    debug::check_expr(fs::exists(path),
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
    // Check that file path exists
    debug::check_expr(fs::exists(path),
      fmt::format("failed to resolve path \"{}\"", path.string()));
      
    // Attempt to open file stream
    std::ifstream ifs(path, std::ios::ate);
    debug::check_expr(ifs.is_open(),
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
    // Attempt to open output file stream in text mode
    std::ofstream ofs(path, std::ios::out);
    debug::check_expr(ofs.is_open(),
      fmt::format("failed to open file \"{}\"", path.string()));

    // Write string directly to file in text mode
    ofs.write(str.data(), str.size());
    ofs.close();
  }
} // namespace met::io