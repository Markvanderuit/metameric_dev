#include <metameric/core/image.hpp>
#include <metameric/core/ranges.hpp>
#include <cstdint>
#include <limits>
#include <format>
#include <type_traits>
#include <functional>
#include <unordered_map>

// Block of loader includes
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYEXR_USE_MINIZ  1
#define TINYEXR_USE_OPENMP 1
#ifdef _WIN32
#define NOMINMAX
#endif
#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

namespace met {
  namespace detail {
    constexpr uint size_from_format(Image::PixelFormat fmt) {
      switch (fmt) {
        case Image::PixelFormat::eRGB:   return 3;
        case Image::PixelFormat::eRGBA:  return 4;
        case Image::PixelFormat::eAlpha: return 1;
        default:                         return 0;
      }      
    }

    constexpr uint size_from_type(Image::PixelType ty) {
      switch (ty) {
        case Image::PixelType::eUChar:  return static_cast<uint>(sizeof( uchar  ));
        case Image::PixelType::eUShort: return static_cast<uint>(sizeof( ushort ));
        case Image::PixelType::eUInt:   return static_cast<uint>(sizeof( uint   ));
        case Image::PixelType::eFloat:  return static_cast<uint>(sizeof( float  ));
        default:                        return 0;
      }
    }

    constexpr Image::PixelFormat format_from_size(uint size) {
      switch (size) {
        case  3: return Image::PixelFormat::eRGB;
        case  4: return Image::PixelFormat::eRGBA;
        case  1: return Image::PixelFormat::eAlpha;
        default: return Image::PixelFormat::eAlpha;
      }
    }

    constexpr bool is_type_float(Image::PixelType ty)   { return ty == Image::PixelType::eFloat; }
    constexpr bool is_type_integer(Image::PixelType ty) { return ty != Image::PixelType::eFloat; }

    // Default value conversion; probably compiled away as pass-through
    template <typename OTy, typename ITy> 
    requires (std::is_same_v<OTy, ITy>)
    OTy convert(ITy v) {
      return v;
    }

    // Int-to-int type conversion; adjust for different sizes
    template <typename OTy, typename ITy>
    requires (!std::is_same_v<OTy, ITy> && std::is_integral_v<ITy> && std::is_integral_v<OTy>)
    OTy convert(ITy v) {
      uint vi = std::clamp(static_cast<uint>(v),
                           static_cast<uint>(std::numeric_limits<OTy>::min()),
                           static_cast<uint>(std::numeric_limits<OTy>::max()));
      return static_cast<OTy>(vi);
    }

    // Int-to-float conversion
    template <typename OTy, typename ITy>
    requires (std::is_integral_v<ITy> && std::is_floating_point_v<OTy>)
    OTy convert(ITy v) {
      uint i = convert<uint>(v);
      return static_cast<float>(i) / static_cast<OTy>(std::numeric_limits<ITy>::max());
    }

    // Float-to-int conversion
    template <typename OTy, typename ITy>
    requires (std::is_floating_point_v<ITy> && std::is_integral_v<OTy>)
    OTy convert(ITy v) {
      uint i = static_cast<uint>(v * static_cast<ITy>(std::numeric_limits<OTy>::max()));
      return convert<OTy>(i);
    }

    constexpr inline
    void convert_to_float(Image::PixelType type, const std::byte &in, float &out) {
      using PixelType = Image::PixelType;
      switch (type) {
        case PixelType::eUChar:  out = convert<float>(*reinterpret_cast<const uchar *>(&in));  break;
        case PixelType::eUShort: out = convert<float>(*reinterpret_cast<const ushort *>(&in)); break;
        case PixelType::eUInt:   out = convert<float>(*reinterpret_cast<const uint *>(&in));   break;
        case PixelType::eFloat:  out = *reinterpret_cast<const float *>(&in);                  break;
      } // switch (type)
    }

    constexpr inline
    void convert_fr_float(Image::PixelType type, const float &in, std::byte &out) {
      using PixelType = Image::PixelType;
      switch (type) {
        case PixelType::eUChar:  *reinterpret_cast<uchar *>(&out)  = detail::convert<uchar>(in);  break;
        case PixelType::eUShort: *reinterpret_cast<ushort *>(&out) = detail::convert<ushort>(in); break;
        case PixelType::eUInt:   *reinterpret_cast<uint *>(&out)   = detail::convert<uint>(in);   break;
        case PixelType::eFloat:  *reinterpret_cast<float *>(&out)  = in;                          break;
      } // switch (type)
    }

    Colr convert_colr_frmt_to_xyz(Image::ColorFormat type_in, const Colr &c) {
      using ColorFormat = Image::ColorFormat;
      switch (type_in) {
        case ColorFormat::eLRGB: return lrgb_to_xyz(c);
        case ColorFormat::eSRGB: return srgb_to_xyz(c);
        default:                 return c;
      } // switch (type_in)
    }

    Colr convert_xyz_to_colr_frmt(Image::ColorFormat type_out, const Colr &c) {
      using ColorFormat = Image::ColorFormat;
      switch (type_out) {
        case ColorFormat::eLRGB: return xyz_to_lrgb(c);
        case ColorFormat::eSRGB: return xyz_to_srgb(c);
        default:                 return c;
      } // switch (type_in)
    }

    Colr convert_colr_frmt(Image::ColorFormat type_in, Image::ColorFormat type_out, const Colr &c) {
      return convert_xyz_to_colr_frmt(type_out, convert_colr_frmt_to_xyz(type_in, c));
    }
  } // namespace detail

  Image::Image(LoadInfo info) {
    met_trace();
    
    // Check that file path exists
    debug::check_expr(fs::exists(info.path),
      fmt::format("failed to resolve image path \"{}\"", info.path.string()));

    // Placeholder path string for c-style APIs
    std::string spath = info.path.string();
    const char *cpath = spath.c_str();
    
    if (info.path.extension() == ".exr") { /* Attempt load using TinyEXR */
      EXRVersion exr_version;
      EXRHeader  exr_header;
      EXRImage   exr_image;

      const char *error = nullptr;

      // Attempt to read EXR version data
      if (int ret = ParseEXRVersionFromFile(&exr_version, cpath); ret) {
        debug::check_expr(false,
          std::format("Could not parse EXR version, image path \"{}\", return code \"{}\"", info.path.string(), ret));
      }

      // Attempt to read EXR header data
      if (int ret = ParseEXRHeaderFromFile(&exr_header, &exr_version, cpath, &error); ret) {
        std::string err_copy(error);
        FreeEXRErrorMessage(error);
        debug::check_expr(false,
          std::format("Could not parse EXR header, image path \"{}\", error \"{}\"", info.path.string(), err_copy));
      }

      // Read HALF channel as FLOAT
      for (int i = 0; i < exr_header.num_channels; i++) {
        if (exr_header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {
          exr_header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
        }
      }

      // Attempt to load EXR image data
      InitEXRImage(&exr_image);
      if (int ret = LoadEXRImageFromFile(&exr_image, &exr_header, cpath, &error); ret) {
        std::string err_copy(error);
        FreeEXRErrorMessage(error);
        debug::check_expr(false,
          std::format("Could not load EXR image, image path \"{}\", error \"{}\"", info.path.string(), err_copy));
      }

      // Note; here be the assumptions because I really don't have time for this
      // - EXR is not in tiled format
      // - All image layers have the same size and type of data

      // Parse header/image configuration
      m_pixel_type = PixelType::eFloat; // half-data is upcast by TinyEXR
      m_pixel_frmt = detail::format_from_size(exr_image.num_channels);
      m_color_frmt = m_pixel_frmt == PixelFormat::eAlpha ? ColorFormat::eNone : ColorFormat::eLRGB;
      m_size = { exr_image.width, exr_image.height };
      if (exr_header.pixel_types[0] == TINYEXR_PIXELTYPE_UINT)
        m_pixel_type = PixelType::eUInt;
      
      fmt::print("num_channels : {}\n", exr_image.num_channels);
      
      // Allocate image memory
      m_data.resize(m_size.prod() * detail::size_from_format(m_pixel_frmt) 
                                  * detail::size_from_type(m_pixel_type));

      // Used sizes and offsets
      uint type_size = detail::size_from_type(pixel_type());
      uint pixl_chan = detail::size_from_format(pixel_frmt());
      uint pixl_size = pixl_chan * type_size;

      // Attempt to scatter EXR data to image memory
      std::unordered_map<std::string, uint> channel_indices = {
        { "R", 0 }, { "G", 1 }, { "B", 2 }, { "A", 3 },
      };
      for (uint c = 0; c < pixl_chan; ++c) {
        size_t         chan_offs = channel_indices[exr_header.channels[c].name];
        unsigned char* chan_data = exr_image.images[c];

        #pragma omp parallel for
        for (int i = 0; i < m_size.prod(); ++i) {
          size_t src_offs = type_size * i;
          size_t dst_offs = type_size * (i * pixl_chan + chan_offs);
          std::memcpy(&m_data[dst_offs], &chan_data[src_offs], type_size);
        } // for (uint i)
      } // for (uint c)

      fmt::print("Attempted to read: channels = {}, width = {}, height = {}, tiles = {}\n",
        exr_image.num_channels, exr_image.width, exr_image.height, exr_image.num_tiles);
      for (uint i = 0; i < exr_header.num_channels; ++i)
        fmt::print("\tchannel {}, named {}\n", i, exr_header.channels[i].name);

      // Cleanup
      FreeEXRImage(&exr_image);
      FreeEXRHeader(&exr_header);
    } else { /* Fall back to STBImage */
      // Attempt load of sdr .bmp/.png/.jpg file
      int w, h, c;
      std::byte *ptr = (std::byte *) stbi_load(cpath, &w, &h, &c, 0);
      
      // Specify header/image configuration
      m_pixel_type = PixelType::eUChar;
      m_pixel_frmt = detail::format_from_size(c);
      m_color_frmt = m_pixel_frmt == PixelFormat::eAlpha ? ColorFormat::eNone : ColorFormat::eSRGB;
      m_size       = { w, h };

      // Allocate image memory and copy image data over
      size_t data_len = m_size.prod() * detail::size_from_format(m_pixel_frmt) 
                                      * detail::size_from_type(m_pixel_type);
      m_data = std::vector<std::byte>(ptr, ptr + data_len);

      // Cleanup
      stbi_image_free(ptr);
    }
  }

  Image::Image(CreateInfo info)
  : m_pixel_frmt(info.pixel_frmt),
    m_pixel_type(info.pixel_type),
    m_color_frmt(info.color_frmt),
    m_size(info.size),
    m_data(m_size.prod() * detail::size_from_format(info.pixel_frmt) 
                         * detail::size_from_type(info.pixel_type)) {
    met_trace();

    // Do not allow color formats over single-value images
    if (m_pixel_frmt == PixelFormat::eAlpha)
      m_color_frmt = ColorFormat::eNone;

    // Either copy data over, or 
    // If data is provided, run a copy
    if (!info.data.empty())
      std::copy(std::execution::par_unseq, range_iter(info.data), m_data.begin());
  }

  void Image::set_pixel(const eig::Array2u &xy, eig::Array4f v, ColorFormat input_frmt) {
    uint i = xy.y() * m_size.x() + xy.x();

    uint type_size  = detail::size_from_type(m_pixel_type);
    uint frmt_size  = detail::size_from_format(m_pixel_frmt);
    auto src_range = eig::Array4u(0, 1, 2, 3).head(frmt_size).eval();
    auto dst_range  = (type_size * (i * frmt_size + src_range)).eval();

    if (input_frmt != ColorFormat::eNone && m_color_frmt != ColorFormat::eNone)
      v.head<3>() = detail::convert_colr_frmt(m_color_frmt, input_frmt, v.head<3>());

    for (auto [src, dst] : vws::zip(src_range, dst_range))
      detail::convert_fr_float(m_pixel_type, v[src], m_data[dst]);
  }

  eig::Array4f Image::get_pixel(const eig::Array2u &xy, ColorFormat output_frmt) const {
    uint i = xy.y() * m_size.x() + xy.x();

    uint type_size  = detail::size_from_type(m_pixel_type);
    uint frmt_size  = detail::size_from_format(m_pixel_frmt);
    auto dst_range  = eig::Array4u(0, 1, 2, 3).head(frmt_size).eval();
    auto src_range  = (type_size * (i * frmt_size + dst_range)).eval();

    eig::Array4f v = 0.f;
    for (auto [src, dst] : vws::zip(src_range, dst_range))
      detail::convert_to_float(m_pixel_type, m_data[src], v[dst]);

    if (output_frmt != ColorFormat::eNone && m_color_frmt != ColorFormat::eNone)
      v.head<3>() = detail::convert_colr_frmt(m_color_frmt, output_frmt, v.head<3>());
    
    return v;
  }

  eig::Array4f Image::sample(const eig::Array2f &uv, ColorFormat output_frmt) const {
    constexpr auto fmod = [](float f) { return std::fmodf(f, 1.f); };

    eig::Array2f xy   = (uv.unaryExpr(fmod) * m_size.cast<float>() - 0.5f).cwiseMax(0.f).eval();
    eig::Array2f lerp = xy - xy.floor();
    eig::Array2u minv = xy.floor().cast<uint>();
    eig::Array2u maxv = xy.ceil().cast<uint>();

    auto a = get_pixel({ minv.x(), minv.y() }, output_frmt) * (1.f - lerp.x());
    auto b = get_pixel({ maxv.x(), minv.y() }, output_frmt) * lerp.x();
    auto c = get_pixel({ minv.x(), maxv.y() }, output_frmt) * (1.f - lerp.x());
    auto d = get_pixel({ maxv.x(), maxv.y() }, output_frmt) * lerp.x();
    
    return (1.f - lerp.y()) * (a + b) + lerp.y() * (c + d);
  }

  Image Image::convert(ConvertInfo info) const {
    met_trace();

    // Initialize output image in requested format
    Image output = {{ 
      .pixel_frmt = info.pixel_frmt.value_or(m_pixel_frmt),
      .pixel_type = info.pixel_type.value_or(m_pixel_type),
      .color_frmt = info.color_frmt.value_or(m_color_frmt),
      .size       = info.resize_to.isZero() ? m_size : info.resize_to
    }};

    // Used sizes, offsets, misc
    uint src_channel_count = detail::size_from_format(m_pixel_frmt);
    uint dst_channel_count = detail::size_from_format(output.m_pixel_frmt);
    uint src_size          = detail::size_from_type(m_pixel_type);
    uint dst_size          = detail::size_from_type(output.m_pixel_type);

    // Range over overlapping channels
    auto ovl_channels = eig::Array4u(0, 1, 2, 3).head(std::min(src_channel_count, dst_channel_count)).eval();

    // Color format conversion helper
    using namespace std::placeholders;
    bool convert_colr = output.m_color_frmt != m_color_frmt && output.m_pixel_frmt != PixelFormat::eAlpha;
    auto convert_func = std::bind(detail::convert_colr_frmt, m_color_frmt, output.m_color_frmt, _1);

    // Perform transfer with conversion
    if (output.size().isApprox(m_size)) {
      // Images are same in size; perform full transfer. It's more efficient to 
      // handle per-pixel conversion here, than with set_pixel(get_pixel())
      #pragma omp parallel for
      for (int i = 0; i < output.size().prod(); ++i) {
        // Range over input/output channels
        auto src_channels = (src_size * (ovl_channels + i * src_channel_count)).eval();
        auto dst_channels = (dst_size * (ovl_channels + i * dst_channel_count)).eval();

        // Float data is used as a in-between format for conversion
        eig::Array4f f = 0;

        // Gather converted input to float representation
        for (auto [src, ovl] : vws::zip(src_channels, ovl_channels))
          detail::convert_to_float(m_pixel_type, m_data[src], f[ovl]);

        // Apply color space conversion on float representation, to the first three channels **only**
        if (convert_colr)
          f.head<3>() = convert_func(f.head<3>());
        
        // Scatter float representation to converted output
        for (auto [ovl, dst] : vws::zip(ovl_channels, dst_channels))
          detail::convert_fr_float(output.m_pixel_type, f[ovl], output.m_data[dst]);
      } // for (int i)
    } else {
      // Images differ in size; perform resample
      #pragma omp parallel for
      for (int y = 0; y < output.size().y(); ++y) {
        for (int x = 0; x < output.size().x(); ++x) {
          eig::Array2u pixel  = { x, y };
          eig::Array2f uv     = ((pixel.cast<float>() + 0.5f) / output.size().cast<float>());
          output.set_pixel(pixel, sample(uv, output.m_color_frmt));
        } // for (int x)
      } // for (int y)
    }

    return output;
  }

  uint Image::channels() const {
    return detail::size_from_format(m_pixel_frmt);
  }
  
  void Image::save_exr(fs::path path) const {
    met_trace();
    debug::check_expr(channels() == 4, "non-rgba export not implemented");
    debug::check_expr(pixel_type() == PixelType::eFloat, "non-float export not implemented");
    
    // Placeholder path string for c-style APIs
    std::string spath = io::path_with_ext(path, "exr").string();
    const char *cpath = spath.c_str();

    // Initialize components
    EXRHeader  exr_header;
    EXRImage   exr_image;
    InitEXRHeader(&exr_header);
    InitEXRImage(&exr_image);

    uint type_size = detail::size_from_type(pixel_type());
    uint pixl_chan = detail::size_from_format(pixel_frmt());

    // Split into a/b/g/r/ data lines and flip the image
    std::array<std::vector<float>, 4> rgba;
    std::array<unsigned char *, 4>    rgba_p;
    rgba.fill(std::vector<float>(m_size.prod()));
    for (uint c = 0; c < 4; ++c) {
      #pragma omp parallel for
      for (int j = 0; j < m_size.y(); ++j) {
        for (int i = 0; i < m_size.x(); ++i) {
          size_t src_indx = ((m_size.y() - 1 - j) * m_size.x()) + i; // flip y-coord
          size_t dst_indx = j * m_size.x() + i; 
          size_t src_offs = type_size * (src_indx * 4 + c);
          std::memcpy(&rgba[c][dst_indx], &m_data[src_offs], type_size);
        } // for (uint i)
      } // for (uint j)

      // Flip channel pointer so we output as abgr
      rgba_p[c] = (unsigned char *) rgba[3 - c].data();
    } // for (uint c)

    // Specify layout
    exr_image.num_channels = 4;
    exr_image.width        = m_size.x();
    exr_image.height       = m_size.y();
    exr_image.images       = rgba_p.data();

    std::array<EXRChannelInfo, 4> header_channels;
    std::array<int, 4> header_pixel_types = {
      TINYEXR_PIXELTYPE_FLOAT, TINYEXR_PIXELTYPE_FLOAT, TINYEXR_PIXELTYPE_FLOAT, TINYEXR_PIXELTYPE_FLOAT
    };
    std::array<int, 4> requested_pixel_types = {
      TINYEXR_PIXELTYPE_HALF, TINYEXR_PIXELTYPE_HALF, TINYEXR_PIXELTYPE_HALF, TINYEXR_PIXELTYPE_HALF
    };

    std::strcpy(header_channels[0].name, "A\0");
    std::strcpy(header_channels[1].name, "B\0");
    std::strcpy(header_channels[2].name, "G\0");
    std::strcpy(header_channels[3].name, "R\0");
    exr_header.num_channels          = 4;
    exr_header.channels              = header_channels.data();
    exr_header.pixel_types           = header_pixel_types.data();
    exr_header.requested_pixel_types = requested_pixel_types.data();
    
    // Attempt save
    const char *error = nullptr;
    if (int ret = SaveEXRImageToFile(&exr_image, &exr_header, cpath, &error); ret)
      debug::check_expr(false,
        std::format("Could not save to EXR, image path \"{}\", return code \"{}\"", path.string(), ret));
  }
} // namespace met