#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <algorithm>
#include <execution>
#include <vector>

namespace met {
  namespace detail {
    auto v3_to_v4 = [](const auto &v) { return (eig::Array4f() << v, 1).finished(); };
    auto v4_to_v3 = [](const auto &v) { return v.head<3>(); };
  } // namespace detail

  template <typename T, uint D>
  TextureBlock<T, D>::TextureBlock(TextureCreateInfo info) 
  : m_size(info.size), m_data(info.size.prod()) {
    guard(!info.data.empty());
    std::copy(std::execution::par_unseq, info.data.begin(), info.data.end(), m_data.begin());
  }

  namespace io {
    template <typename T>
    Texture2d<T> load_texture2d(const fs::path &path) {
      // Check that file path exists
      debug::check_expr(fs::exists(path),
        fmt::format("failed to resolve path \"{}\"", path.string()));

      // Load image float data from disk
      eig::Array2i v;
      int c;

      // float *data_ptr  = stbi_loadf(path.string().c_str(), &v.x(), &v.y(), &c, 0);
      std::byte *data_ptr  = (std::byte *) stbi_load(path.string().c_str(), &v.x(), &v.y(), &c, 0);
      size_t     data_size = v.prod() * c;

      // Test if data was loaded
      debug::check_expr(data_ptr, 
        fmt::format("failed to load file \"{}\"", path.string()));

      // Initialize texture object with correct size and requested channel layout
      Texture2d<T> texture = {{ .size = v }};
      std::span<T> texture_span = texture.data();

      // Convert data to floats
      std::span<std::byte> data_byte = { data_ptr, data_size };
      std::vector<float>   data_float(data_byte.size());
      std::transform(std::execution::par_unseq,
        data_byte.begin(), data_byte.end(), data_float.begin(),
        [](std::byte b) { return static_cast<float>(b) / 255.f; });

      // Perform channel-correct copy/transform into texture data
      if (c == 3) {
        auto arr_span = as_span<const eig::Array3f>(data_float);
        if constexpr (std::is_same_v<T, eig::Array4f>) {
          std::transform(std::execution::par_unseq, 
            arr_span.begin(), arr_span.end(), texture_span.begin(), detail::v3_to_v4);
        } else {
          std::copy(std::execution::par_unseq,
            arr_span.begin(), arr_span.end(), texture_span.begin());
        }
      } else if (c == 4) {
        auto arr_span = as_span<const eig::Array4f>(data_float);
        if constexpr (std::is_same_v<T, eig::Array4f>) {
          std::copy(std::execution::par_unseq,
            arr_span.begin(), arr_span.end(), texture_span.begin());
        } else {
          std::transform(std::execution::par_unseq, 
            arr_span.begin(), arr_span.end(), texture_span.begin(), detail::v4_to_v3);
        }
      }

      stbi_image_free(data_ptr);
      return texture;
    }

    template <typename T>
    void save_texture2d(const fs::path &path, const Texture2d<T> &texture) {
      const char *pstr = path.string().c_str();
      auto size = texture.size();
      auto c    = Texture2d<T>::dims();
      auto data = cast_span<const float>(texture.data());

      // Convert data to unsigned bytes
      std::vector<std::byte> byte_data(data.size());
      std::transform(std::execution::par_unseq,
        data.begin(), data.end(), byte_data.begin(),
        [](float f) { return static_cast<std::byte>(f * 255.f); });
      
      const auto ext = path.extension();
      int ret = 0;
      if (ext == ".png") {
        ret = stbi_write_png(pstr, size.x(), size.y(), c, byte_data.data(), 8 * 4 * size.x());
      } else if (ext == ".jpg") {
        ret = stbi_write_jpg(pstr, size.x(), size.y(), c, byte_data.data(), 0);
      } else if (ext == ".bmp") {
        ret = stbi_write_bmp(pstr, size.x(), size.y(), c, byte_data.data());
      } else {
        debug::check_expr(false,
          fmt::format("unsupported image extension for writing \"{}\"", path.string()));
      }

      debug::check_expr(ret != 0,
        fmt::format("could not save file to \"{}\", code was {}", path.string(), ret));
    }
    
    Texture2d3f as_unaligned(const Texture2d3f_al &in) {
      Texture2d3f out = {{ .size = in.size() }};
      std::copy(std::execution::par_unseq, in.data().begin(), in.data().end(), out.data().begin());
      return out;
    }

    Texture2d3f_al as_aligned(const Texture2d3f &in) {
      Texture2d3f_al out = {{ .size = in.size() }};
      std::copy(std::execution::par_unseq, in.data().begin(), in.data().end(), out.data().begin());
      return out;
    }
    
    template <typename T>
    void to_srgb(Texture2d<T> &lrgb) {
      auto d = lrgb.data();
      std::transform(std::execution::par_unseq,
          d.begin(), d.end(), d.begin(), linear_srgb_to_gamma_srgb);
    }

    template <typename T>
    void to_lrgb(Texture2d<T> &srgb) {
      auto d = srgb.data();
      std::transform(std::execution::par_unseq,
          d.begin(), d.end(), d.begin(), gamma_srgb_to_linear_srgb);
    }
    
    // Explicit instantiation skipping alpha channels
    template <>
    void to_srgb(Texture2d<eig::Array4f> &lrgb) {
      auto d = lrgb.data();
      std::transform(std::execution::par_unseq, 
        d.begin(), d.end(), d.begin(), [](eig::Array4f v) {
          v.head<3>() = linear_srgb_to_gamma_srgb(v.head<3>());
          return v;
      });
    }
    
    // Explicit instantiation skipping alpha channels
    template <>
    void to_lrgb(Texture2d<eig::Array4f> &srgb) {
      auto d = srgb.data();
      std::transform(std::execution::par_unseq, 
        d.begin(), d.end(), d.begin(), [](eig::Array4f v) {
          v.head<3>() = gamma_srgb_to_linear_srgb(v.head<3>());
          return v;
      });
    }
    
    template <typename T>
    Texture2d<T> as_srgb(const Texture2d<T> &lrgb) {
      Texture2d<T> obj(lrgb);
      to_srgb(obj);
      return obj;
    }
    
    template <typename T>
    Texture2d<T> as_lrgb(const Texture2d<T> &srgb) {
      Texture2d<T> obj(srgb);
      to_lrgb(obj);
      return obj;
    }

    /* Explicit template instantiations for common types */

    template void to_srgb<eig::Array3f>(Texture2d<eig::Array3f> &);
    template void to_srgb<eig::AlArray3f>(Texture2d<eig::AlArray3f> &);

    template void to_lrgb<eig::Array3f>(Texture2d<eig::Array3f> &);
    template void to_lrgb<eig::AlArray3f>(Texture2d<eig::AlArray3f> &);

    template Texture2d<eig::Array3f> as_srgb<eig::Array3f>(const Texture2d<eig::Array3f> &);
    template Texture2d<eig::Array4f> as_srgb<eig::Array4f>(const Texture2d<eig::Array4f> &);
    template Texture2d<eig::AlArray3f> as_srgb<eig::AlArray3f>(const Texture2d<eig::AlArray3f> &);
    
    template Texture2d<eig::Array3f> as_lrgb<eig::Array3f>(const Texture2d<eig::Array3f> &);
    template Texture2d<eig::Array4f> as_lrgb<eig::Array4f>(const Texture2d<eig::Array4f> &);
    template Texture2d<eig::AlArray3f> as_lrgb<eig::AlArray3f>(const Texture2d<eig::AlArray3f> &);

    template Texture2d<eig::Array3f>   load_texture2d<eig::Array3f>(const fs::path &);
    template Texture2d<eig::Array4f>   load_texture2d<eig::Array4f>(const fs::path &);
    
    template void save_texture2d<eig::Array3f>(const fs::path &, const Texture2d<eig::Array3f> &);
    template void save_texture2d<eig::Array4f>(const fs::path &, const Texture2d<eig::Array4f> &);
  } // namespace io
} // namespace met