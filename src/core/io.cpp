#include <metameric/core/io.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/detail/glm.hpp>
#include <highfive/H5File.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <algorithm>
#include <execution>
#include <ranges>

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

  TextureData<std::byte> load_texture_byte(std::filesystem::path path) {
    // Check that file path exists
    debug::check_expr(std::filesystem::exists(path),
      fmt::format("failed to resolve path \"{}\"", path.string()));

    // Holder object that is returned later
    TextureData<std::byte> obj;

    // Attempt to load texture data, passing size values into holder object
    auto *ptr = stbi_load(path.string().c_str(), &obj.size[0], &obj.size[1], &obj.channels, 0);
    debug::check_expr(ptr, fmt::format("failed to load file \"{}\"", path.string()));

    // Copy loaded data into holder object
    obj.data.assign((std::byte *) ptr, (std::byte *) ptr + glm::prod(obj.size) * obj.channels);
    
    stbi_image_free(ptr);
    return obj;
  }

  TextureData<float> load_texture_float(std::filesystem::path path) {
    // Check that file path exists
    debug::check_expr(std::filesystem::exists(path),
      fmt::format("failed to resolve path \"{}\"", path.string()));

    // Holder object that is returned later
    TextureData<float> obj;

    // Attempt to load texture data, passing size values into holder object
    float *ptr = stbi_loadf(path.string().c_str(), &obj.size[0], &obj.size[1], &obj.channels, 0);
    debug::check_expr(ptr, fmt::format("failed to load file \"{}\"", path.string()));

    // Copy loaded data into holder object
    obj.data.assign(ptr, ptr + glm::prod(obj.size) * obj.channels);

    stbi_image_free(ptr);
    return obj;
  }

  void write_texture_float(const TextureData<float> obj, std::filesystem::path path) {
    const auto ext = path.extension();
    int ret = 0;
    if (ext == "png") {
      ret = stbi_write_png(path.string().c_str(), obj.size.x, obj.size.y, obj.channels,
        obj.data.data(), 8 * 4 * obj.size.x);
    } else if (ext == "jpg") {
      ret = stbi_write_jpg(path.string().c_str(), obj.size.x, obj.size.y, obj.channels,
        obj.data.data(), 0);
    } else if (ext == "bmp") {
      ret = stbi_write_bmp(path.string().c_str(), obj.size.x, obj.size.y, obj.channels,
        obj.data.data());
    } else {
      debug::check_expr(false,
        fmt::format("unsupported image extension for writing \"{}\"", path.string()));
    }
  }

  // TODO: Remove once done with testing
  SpectralData load_spectral_data_hd5(std::filesystem::path path) {
    // Check that file path exists
    debug::check_expr(std::filesystem::exists(path),
      fmt::format("failed to resolve path \"{}\"", path.string()));

    SpectralData obj;

    // Attempt to read file, then extract forcibly named dataset from file
    HighFive::File file(path.string(), HighFive::File::ReadOnly);
    HighFive::DataSet ds = file.getDataSet("TotalRefs");
    ds.read(obj.data);

    obj.data = detail::transpose(obj.data);
    obj.size = obj.data.size();
    obj.channels = obj.data[0].size();

    return obj;
  }

  void apply_channel_conversion(TextureData<float> &obj, uint new_channels, float new_value) {
    std::vector<float> new_data((obj.data.size() / obj.channels) * new_channels, new_value);

    #pragma omp parallel for
    for (uint i = 0; i < obj.data.size() / obj.channels; ++i) {
      for (uint j = 0; j < obj.channels; ++j) {
        new_data[i * new_channels + j] = obj.data[i * obj.channels + j];
      }
    }

    obj.data = std::move(new_data);
    obj.channels = new_channels;
  }
  
  void apply_srgb_to_lrgb(TextureData<float> &obj, bool skip_alpha) {
    // Wrapper function to obtain a view over a subset of data using indices
    constexpr auto ref_i = [](auto &v) { return [&v](const auto i) -> float& { return v[i]; }; };

    // Wrapper function to selectively skip alpha channels
    auto skip_i = [&](size_t i) { return !skip_alpha || obj.channels < 4 || (i + 1) % 4 != 0; };

    // Determine data (subset) to operate on, skipping alpha channels
    auto &data   = obj.data;
    auto indices = std::views::iota(0ul, data.size()) | std::views::filter(skip_i);
    auto indexed = indices | std::views::transform(ref_i(data));

    // Apply srgb->linear transformation to data
    std::ranges::transform(indexed, indexed.begin(), gamma_srgb_to_linear_srgb<float>);
  }

  void apply_lrgb_to_srgb(TextureData<float> &obj, bool skip_alpha) {
    // Wrapper function to obtain a view over a subset of data using indices
    constexpr auto ref_i = [](auto &v) { return [&v](const auto i) -> float& { return v[i]; }; };

    // Wrapper function to selectively skip alpha channels
    auto skip_i = [&](size_t i) { return !skip_alpha || obj.channels < 4 || (i + 1) % 4 != 0; };

    // Determine data (subset) to operate on, skipping alpha channels
    auto &data = obj.data;
    auto indices = std::views::iota(0ul, data.size()) | std::views::filter(skip_i);
    auto indexed = indices | std::views::transform(ref_i(data));

    // Apply linear->srgb transformation to data
    std::ranges::transform(indexed, indexed.begin(), linear_srgb_to_gamma_srgb<float>);
  }
} // namespace met::io