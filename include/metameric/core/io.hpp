#pragma once

#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/glm.hpp>
#include <filesystem>

namespace met::io {
  // Return object for load_texture_*(...) below
  template <typename Ty>
  struct TextureData {
    std::vector<Ty> data;
    glm::ivec2 size;
    int channels;
  };

  // Return object for load_spectral_data_hd5(...) below
  struct SpectralData {
    std::vector<std::vector<float>> data;
    size_t size;
    size_t channels;
  };

  // Load raw texture data from the given filepath
  TextureData<std::byte> load_texture_byte(std::filesystem::path path);
  
  // Load float-scaled texture data from the given filepath
  TextureData<float> load_texture_float(std::filesystem::path path);

  // Load color object texture data from the given filepath
  TextureData<Color> load_texture_color(std::filesystem::path path);

  // Write float-scaled texture data to the given filepath
  void write_texture_float(const TextureData<float> obj, std::filesystem::path path);

  // Load raw spectral database from the given hd5 filepath
  SpectralData load_spectral_data_hd5(std::filesystem::path path);

  // Insert/strip channels from srgb texture data
  void apply_channel_conversion(TextureData<float> &obj, uint new_channels, float new_value);

  // Linearize/delinearize srgb texture data
  void apply_srgb_to_lrgb(TextureData<Color> &obj, bool skip_alpha = true);
  void apply_lrgb_to_srgb(TextureData<Color> &obj, bool skip_alpha = true);
} // namespace met::io