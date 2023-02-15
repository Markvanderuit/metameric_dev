#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <algorithm>
#include <execution>
#include <vector>

namespace met {
  namespace detail {
    auto v3_to_v4 = [](auto v) { return (eig::Array<decltype(v)::Scalar, 4, 1>() << v, 1).finished(); };
    auto v4_to_v3 = [](auto v) { return v.head<3>(); };
  } // namespace detail

  template <typename T, uint D>
  TextureBlock<T, D>::TextureBlock(TextureCreateInfo info) 
  : m_size(info.size), m_data(info.size.prod()) {
    met_trace();
    met_trace_alloc(m_data.data(), m_data.size() * sizeof(T));

    if (!info.data.empty())
      std::copy(std::execution::par_unseq, range_iter(info.data), m_data.begin());
  }

  template <typename T, uint D>
  TextureBlock<T, D>::~TextureBlock() {
    met_trace();
    met_trace_free(m_data.data());
  }

  template <typename T> 
  template <typename T_> 
  Texture2d<T_> Texture2d<T>::convert() {
    using IT = T::Scalar;
    using OT = T_::Scalar;

    Texture2d<T_> texture = {{ .size = this->size() }};

    if constexpr (std::is_same_v<IT, OT>) {
      std::copy(std::execution::par_unseq, range_iter(this->data()), texture.data().begin());
    } else if constexpr (!std::is_same_v<IT, float> && std::is_same_v<OT, float>) {
      // Convert to floating precision
      float max_v = 1.0 / static_cast<float>(std::pow(2ul, sizeof(IT)) - 1);
      std::transform(std::execution::par_unseq, range_iter(this->data()), texture.data().begin(), 
      [&](const auto &iv) {
        return iv.cast<OT>() * max_v;
      });
    } else if constexpr (std::is_same_v<IT, float> && !std::is_same_v<OT, float>) {
      // Convert to fixed precision
      float max_v = static_cast<float>(std::pow(2ul, sizeof(IT)) - 1);
      std::transform(std::execution::par_unseq, range_iter(this->data()), texture.data().begin(), 
      [&](const auto &iv) {
        return (iv.min(0.0).max(1.0) * max_v).cast<OT>();
      });
    }

    return texture;
  }

  namespace io {
    template <typename T>
    Texture2d<T> load_texture2d(const fs::path &path, bool srgb_to_lrgb) {
      constexpr uint C_ = T::RowsAtCompileTime; 
      using          T_ = eig::Array<float, C_, 1>;

      met_trace();

      // Check that file path exists
      debug::check_expr_dbg(fs::exists(path),
        fmt::format("failed to resolve path \"{}\"", path.string()));

      // Load image from disk
      eig::Array2i v; // Size at runtime
      int c;          // Rows at runtime
      std::byte *data_ptr  = (std::byte *) stbi_load(path.string().c_str(), &v.x(), &v.y(), &c, 0);
      size_t     data_size = v.prod() * c;

      // Test if data was loaded
      debug::check_expr_dbg(data_ptr, 
        fmt::format("failed to load file \"{}\"", path.string()));

      // Elevate data to floating point
      std::span<byte>    data_byte = { data_ptr, data_size };
      std::vector<float> data_float(data_byte.size());
      std::transform(std::execution::par_unseq, range_iter(data_byte), data_float.begin(),
        [](std::byte b) { return static_cast<float>(b) / 255.f; });

      // Initialize temporary texture object with correct dims, requested channel layout, and float data
      Texture2d<T_> texture_float = {{ .size = v.cast<uint>() }};
      std::span<T_> texture_span  = texture_float.data();

      // Perform channel-correct copy/transform into texture data
      if (c == 3) {
        auto arr_span = cnt_span<const eig::Array3f>(data_float);
        if constexpr (C_ == 4) {
          std::transform(std::execution::par_unseq, range_iter(arr_span), texture_span.begin(), detail::v3_to_v4);
        } else {
          std::copy(std::execution::par_unseq, range_iter(arr_span), texture_span.begin());
        }
      } else if (c == 4) {
        auto arr_span = cnt_span<const eig::Array4f>(data_float);
        if constexpr (C_ == 4) {
          std::copy(std::execution::par_unseq, range_iter(arr_span), texture_span.begin());
        } else {
          std::transform(std::execution::par_unseq, range_iter(arr_span), texture_span.begin(), detail::v4_to_v3);
        }
      }

      // Release stbi data from this point
      stbi_image_free(data_ptr);

      // Strip linear sRGB gamma if requested
      if (srgb_to_lrgb)
        to_lrgb(texture_float);
      
      // Return result, converting to requested sized internal format
      return texture_float.convert<T>();
    }

    template <typename T>
    void save_texture2d(const fs::path &path, const Texture2d<T> &texture_, bool lrgb_to_srgb) {
      met_trace();

      // Operate on a copy as gamma may need to be applied;
      // apply linear sRGB gamma if requested
      Texture2d<T> texture({ .size = texture_.size(), .data = texture_.data() });
      if (lrgb_to_srgb)
        to_srgb(texture);

      const char *pstr = path.string().c_str();
      auto size = texture.size();
      auto c    = Texture2d<T>::dims();
      auto data = cast_span<const float>(texture.data());

      // Convert data to unsigned bytes
      std::vector<std::byte> byte_data(data.size());
      std::transform(std::execution::par_unseq,
        data.begin(), data.end(), byte_data.begin(),
        [](float f) { 
          auto b = static_cast<std::byte>(std::clamp(f * 256.f, 0.f, 255.f));
          // fmt::print("{} -> {}\n", b, f);
          return b; // static_cast<std::byte>(std::max(f * 256.f, 0.f)); 
        });
      
      const auto ext = path.extension();
      int ret = 0;
      if (ext == ".png") {
        ret = stbi_write_png(pstr, size.x(), size.y(), c, byte_data.data(), size.x() * c);
      } else if (ext == ".jpg") {
        ret = stbi_write_jpg(pstr, size.x(), size.y(), c, byte_data.data(), 0);
      } else if (ext == ".bmp") {
        ret = stbi_write_bmp(pstr, size.x(), size.y(), c, byte_data.data());
      } else {
        debug::check_expr_dbg(false,
          fmt::format("unsupported image extension for writing \"{}\"", path.string()));
      }

      debug::check_expr_dbg(ret != 0,
        fmt::format("could not save file to \"{}\", code was {}", path.string(), ret));
    }
    
    Texture2d3f as_unaligned(const Texture2d3f_al &in) {
      met_trace();
      Texture2d3f out = {{ .size = in.size() }};
      std::copy(std::execution::par_unseq, in.data().begin(), in.data().end(), out.data().begin());
      return out;
    }

    Texture2d3f_al as_aligned(const Texture2d3f &in) {
      met_trace();
      Texture2d3f_al out = {{ .size = in.size() }};
      std::copy(std::execution::par_unseq, in.data().begin(), in.data().end(), out.data().begin());
      return out;
    }
    
    template <typename T>
    void to_srgb(Texture2d<T> &lrgb) {
      met_trace();
      auto d = lrgb.data();
      std::transform(std::execution::par_unseq,
          d.begin(), d.end(), d.begin(), lrgb_to_srgb);
    }

    template <typename T>
    void to_lrgb(Texture2d<T> &srgb) {
      met_trace();
      auto d = srgb.data();
      std::transform(std::execution::par_unseq,
          d.begin(), d.end(), d.begin(), srgb_to_lrgb);
    }
    
    // Explicit instantiation skipping alpha channels
    template <>
    void to_srgb(Texture2d<eig::Array4f> &lrgb) {
      met_trace();
      auto d = lrgb.data();
      std::transform(std::execution::par_unseq, 
        d.begin(), d.end(), d.begin(), [](eig::Array4f v) {
          v.head<3>() = lrgb_to_srgb(v.head<3>());
          return v;
      });
    }
    
    // Explicit instantiation skipping alpha channels
    template <>
    void to_lrgb(Texture2d<eig::Array4f> &srgb) {
      met_trace();
      auto d = srgb.data();
      std::transform(std::execution::par_unseq, 
        d.begin(), d.end(), d.begin(), [](eig::Array4f v) {
          v.head<3>() = srgb_to_lrgb(v.head<3>());
          return v;
      });
    }
    
    template <typename T>
    Texture2d<T> as_srgb(const Texture2d<T> &lrgb) {
      met_trace();
      Texture2d<T> obj({ .size = lrgb.size(), .data = lrgb.data() });
      to_srgb(obj);
      return obj;
    }
    
    template <typename T>
    Texture2d<T> as_lrgb(const Texture2d<T> &srgb) {
      met_trace();
      Texture2d<T> obj({ .size = srgb.size(), .data = srgb.data() });
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

    template Texture2d<eig::Array3f> load_texture2d<eig::Array3f>(const fs::path &, bool);
    template Texture2d<eig::Array4f> load_texture2d<eig::Array4f>(const fs::path &, bool);
    
    template void save_texture2d<eig::Array3f>(const fs::path &, const Texture2d<eig::Array3f> &, bool);
    template void save_texture2d<eig::Array4f>(const fs::path &, const Texture2d<eig::Array4f> &, bool);
  } // namespace io

  /* Explicit template instantiations for common types */

  template class Texture2d<eig::Array3f>;
  template class Texture2d<eig::Array4f>;
  template class Texture2d<eig::AlArray3f>;
  // template class Texture2d<eig::Array<std::byte, 3, 1>>;
  // template class Texture2d<eig::Array<std::byte, 4, 1>>;
  // template class Texture2d<eig::AlArray<std::byte, 3>>;
} // namespace met