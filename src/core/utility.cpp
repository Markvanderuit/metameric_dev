#pragma once

#include <metameric/core/utility.hpp>
#include <metameric/core/detail/glm.hpp>
#include <highfive/H5File.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <algorithm>
#include <execution>
#include <ranges>
#include <span>


namespace met {
  namespace detail {
    template <class T, class U>
    constexpr inline
    std::span<T> convert_span(std::span<U> s) {
        auto data = s.data();
        guard(data, { });
        auto bytes = s.size_bytes();
        return { reinterpret_cast<T*>(data), bytes / narrow_cast<int>(sizeof(T)) };
    }

    template <typename Float>
    constexpr inline
    Float srgb_to_lrgb(Float f) {
      if (f <= 0.003130) {
        return f * 12.92;
      } else {
        return 1.055 * std::pow<Float>(f, 1.0 / 2.4) - 0.055;
      }
    }

    template <typename Float>
    constexpr inline
    Float lrgb_to_srgb(Float f) {
      if (f <= 0.04045) {
        return f / 12.92;
      } else {
        return std::pow<Float>((f + 0.055) / 1.055, 2.4);
      }
    }

    template <typename T>
    constexpr inline
    std::vector<std::vector<T>> pivot(const std::vector<std::vector<T>> &v) {
      std::vector<std::vector<T>> r(v[0].size(), std::vector<T>(v.size()));

      #pragma omp parallel for
      for (size_t j = 0; j < v.size(); j++) {
        for (size_t i = 0; i < v[0].size(); i++) {
            r[i][j] = v[j][i];
        }
      }

      return r;
    }
  }

  namespace io {
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

      obj.data = detail::pivot(obj.data);
      obj.size = obj.data.size();
      obj.channels = obj.data[0].size();

      return obj;
    }
    
    void apply_srgb_to_lrgb(TextureData<float> &obj, bool skip_alpha) {
      // Wrapper function to obtain a view over a subset of data using indices
      constexpr auto ref_i = [](auto &v) { return [&v](const auto i) -> float& { return v[i]; }; };

      // Wrapper function to selectively skip alpha channels
      auto skip_i = [&](size_t i) { return (!skip_alpha || obj.channels < 4) || i % 4 == 0ul; };

      // Determine data (subset) to operate on, skipping alpha channels
      auto &data = obj.data;
      auto indices = std::views::iota(0ul, data.size()) | std::views::filter(skip_i);
      auto indexed = indices | std::views::transform(ref_i(data));

      // Apply srgb->linear transformation to data
      std::ranges::transform(indexed, indexed.begin(), detail::srgb_to_lrgb<float>);
    }

    void apply_lrgb_to_srgb(TextureData<float> &obj, bool skip_alpha) {
      // Wrapper function to obtain a view over a subset of data using indices
      constexpr auto ref_i = [](auto &v) { return [&v](const auto i) -> float& { return v[i]; }; };

      // Wrapper function to selectively skip alpha channels
      auto skip_i = [&](size_t i) { return (!skip_alpha || obj.channels < 4) || i % 4 == 0ul; };

      // Determine data (subset) to operate on, skipping alpha channels
      auto &data = obj.data;
      auto indices = std::views::iota(0ul, data.size()) | std::views::filter(skip_i);
      auto indexed = indices | std::views::transform(ref_i(data));

      // Apply linear->srgb transformation to data
      std::ranges::transform(indexed, indexed.begin(), detail::lrgb_to_srgb<float>);
    }
  } // namespace io
} // namespace met