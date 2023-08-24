#include <metameric/core/math.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <algorithm>
#include <execution>
#include <vector>
#include <limits>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define TINYEXR_USE_MINIZ  1
#define TINYEXR_USE_OPENMP 1
#ifdef _WIN32
#define NOMINMAX
#endif
#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

namespace met {
  namespace detail {
    auto v3_to_v4 = [](auto v) { return (eig::Array<decltype(v)::Scalar, 4, 1>() << v, 1).finished(); };
    auto v4_to_v3 = [](auto v) { return v.head<3>(); };

    std::array<std::string, 4> exr_channel_flags = { "B\0", "G\0", "R\0", "A\0" };

    void save_tinyexr(const fs::path &path, std::span<const float> data, uint w, uint h, uint c) {
      met_trace();

      debug::check_expr(c <= 4, "maximum 4 channels supported");
      
      // Scatter data into per-channel blocks
      std::vector<std::vector<float>> blocks(c);
      for (int i = 0; i < c; ++i)
        blocks[i].resize(w * h);
      #pragma omp parallel for
      for (int i = 0; i < data.size(); ++i)
        blocks[(c - 1) - (i % c)][i / c] = data[i]; // reverse-order

      std::vector<float *> block_ptrs;
      std::ranges::transform(blocks, std::back_inserter(block_ptrs), [](auto &block) { return block.data(); });

      std::vector<EXRChannelInfo> channels(c);
      std::vector<int> pixel_types(c);
      std::vector<int> requested_pixel_types(c);
      for (uint i = 0; i < c; ++i) {
        strncpy(channels[i].name, exr_channel_flags[i].data(), exr_channel_flags[i].length());
        pixel_types[i]           = TINYEXR_PIXELTYPE_FLOAT; // Store full precision; no half-precision shenanigans
        requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; // Store full precision; no half-precision shenanigans
      }

      EXRHeader header;
      InitEXRHeader(&header);
      header.num_channels          = c;
      header.channels              = channels.data();
      header.pixel_types           = pixel_types.data();
      header.requested_pixel_types = pixel_types.data();

      EXRImage image;
      InitEXRImage(&image);
      image.width         = w;
      image.height        = h;
      image.num_channels  = c;
      image.images        = (unsigned char **) block_ptrs.data();

      const char *err_code = nullptr;
      const char *pstr = path.string().c_str();
      int ret = SaveEXRImageToFile(&image, &header, pstr, &err_code);
      
      debug::check_expr(ret == TINYEXR_SUCCESS,
        fmt::format("could not save image to \"{}\", code was {}", path.string(), ret));
    }

    void save_stb(const fs::path &path, std::span<const float> data, uint w, uint h, uint c) {
      met_trace();

      debug::check_expr(c <= 4, "maximum 4 channels supported");

      // Convert data to unsigned bytes
      std::vector<std::byte> byte_data(data.size());
      std::transform(std::execution::par_unseq,
        data.begin(), data.end(), byte_data.begin(),
        [](float f) { return static_cast<std::byte>(std::clamp(f * 256.f, 0.f, 255.f)); });
      
      // Get path information
      const auto ext = path.extension();
      const char *pstr = path.string().c_str();

      int ret = 0;
      if (ext == ".png") {
        ret = stbi_write_png(pstr, w, h, c, byte_data.data(), w * c);
      } else if (ext == ".jpg") {
        ret = stbi_write_jpg(pstr, w, h, c, byte_data.data(), 0);
      } else if (ext == ".bmp") {
        ret = stbi_write_bmp(pstr, w, h, c, byte_data.data());
      } else {
        debug::check_expr(false,
          fmt::format("unsupported image extension for writing \"{}\"", path.string()));
      }

      debug::check_expr(ret != 0,
        fmt::format("could not save image to \"{}\", code was {}", path.string(), ret));
    }
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
    Texture2d<T> load_texture2d(const fs::path &path, bool _srgb_to_lrgb) {
      constexpr uint C_ = T::RowsAtCompileTime; 
      using          T_ = eig::Array<float, C_, 1>;

      met_trace();

      // Check that file path exists
      debug::check_expr(fs::exists(path),
        fmt::format("failed to resolve path \"{}\"", path.string()));

      // Strip gamma if requested, but not for .EXR input
      bool srgb_to_lrgb = _srgb_to_lrgb && path.extension() != ".exr";

      eig::Array2i       v;          // Size at runtime
      int                c;          // Rows at runtime
      std::vector<float> data_float; // Data at runtime

      // Load image from disk
      if (path.extension() == ".exr") {
        //  TODO Stop assuming RGBA input format
        c = 4;

        // Load hdr .exr file
        const char *err = nullptr;
        float      *ptr;
        int         ret = LoadEXR(&ptr, &v[0], &v[1], path.string().c_str(), &err); // deprecated :(
        size_t      size = v.prod() * c;

        // Test if data was loaded
        debug::check_expr(ret == TINYEXR_SUCCESS, 
          fmt::format("failed to load file \"{}\"", path.string()));

        // Copy data over
        data_float = std::vector<float>(ptr, ptr + size);

        // Release exr data from this point
        free(ptr);
      } else {
        // Load sdr .bmp/.png/.jpg file
        std::byte *ptr  = (std::byte *) stbi_load(path.string().c_str(), &v.x(), &v.y(), &c, 0);
        size_t     size = v.prod() * c;

        // Test if data was loaded
        debug::check_expr(ptr, 
          fmt::format("failed to load file \"{}\"", path.string()));
            
        // Elevate data to floating point
        data_float.resize(size);
        std::span<byte> data_byte = { ptr, size };
        std::transform(std::execution::par_unseq, range_iter(data_byte), data_float.begin(),
          [](std::byte b) { return static_cast<float>(b) / 255.f; });
          
        // Release stbi data from this point
        stbi_image_free(ptr);
      }

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
      if (lrgb_to_srgb && path.extension().string() != ".exr")
        to_srgb(texture);

      const char *pstr = path.string().c_str();
      auto size = texture.size();
      auto c    = Texture2d<T>::dims();
      auto data = cast_span<const float>(texture.data());

      if (path.extension().string() == ".exr") {
        // Use TinyEXR to store hdr output
        detail::save_tinyexr(path, data, size.x(), size.y(), c);
      } else {
        // Use STB_image_write to store sdr output
        detail::save_stb(path, data, size.x(), size.y(), c);
      }
    }
    
    Texture2d3f as_unaligned(const AlTexture2d3f &in) {
      met_trace();
      Texture2d3f out = {{ .size = in.size() }};
      std::copy(std::execution::par_unseq, in.data().begin(), in.data().end(), out.data().begin());
      return out;
    }

    AlTexture2d3f as_aligned(const Texture2d3f &in) {
      met_trace();
      AlTexture2d3f out = {{ .size = in.size() }};
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