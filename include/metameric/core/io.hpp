#pragma once

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

  // Load raw spectral database from the given hd5 filepath
  SpectralData load_spectral_data_hd5(std::filesystem::path path);

  // Linearize/delinearize srgb texture data
  void apply_srgb_to_lrgb(TextureData<float> &obj, bool skip_alpha = true);
  void apply_lrgb_to_srgb(TextureData<float> &obj, bool skip_alpha = true);
} // namespace met::io