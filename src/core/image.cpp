#include <metameric/core/image.hpp>
#include <cstdint>
#include <limits>
#include <format>
#include <type_traits>
#include <unordered_map>

// Block of loader includes
// #define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYEXR_USE_MINIZ  1
#define TINYEXR_USE_OPENMP 1
#ifdef _WIN32
#define NOMINMAX
#endif
// #define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

namespace met {
  namespace detail {
    constexpr size_t size_from_format(Image::PixelFormat fmt) {
      switch (fmt) {
        case Image::PixelFormat::eRGB:   return 3;
        case Image::PixelFormat::eRGBA:  return 4;
        case Image::PixelFormat::eAlpha: return 1;
        default:                                return 0;
      }      
    }

    constexpr size_t size_from_type(Image::PixelType ty) {
      switch (ty) {
        case Image::PixelType::eUChar:  return sizeof( uchar  );
        case Image::PixelType::eUShort: return sizeof( ushort );
        case Image::PixelType::eUInt:   return sizeof( uint   );
        case Image::PixelType::eFloat:  return sizeof( float  );
        default:                               return 0;
      }
    }

    constexpr Image::PixelFormat format_from_size(size_t size) {
      switch (size) {
        case 3: return Image::PixelFormat::eRGB;
        case 4: return Image::PixelFormat::eRGBA;
        case 1: return Image::PixelFormat::eAlpha;
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
      m_size = { exr_image.width, exr_image.height };
      if (exr_header.pixel_types[0] == TINYEXR_PIXELTYPE_UINT)
        m_pixel_type = PixelType::eUInt;
      
      // Allocate image memory
      m_data.resize(m_size.prod() * detail::size_from_format(m_pixel_frmt) 
                                  * detail::size_from_type(m_pixel_type));

      // Used sizes and offsets
      size_t type_size = detail::size_from_type(type());
      size_t pixl_chan = detail::size_from_format(frmt());
      size_t pixl_size = pixl_chan * type_size;

      // Attempt to scatter EXR data to image memory
      std::unordered_map<std::string, uint> channel_indices = {
        { "R", 0 }, { "G", 1 }, { "B", 2 }, { "A", 3 },
      };
      for (uint c = 0; c < pixl_chan; ++c) {
        size_t         chan_offs = channel_indices[exr_header.channels[c].name];
        unsigned char* chan_data = exr_image.images[c];

        #pragma omp parallel for
        for (int i = 0; i < m_size.prod(); ++i) {
          size_t dst_offs = i * pixl_chan * type_size + chan_offs * type_size;
          size_t src_offs = i * type_size;
          std::memcpy(&m_data[dst_offs], &chan_data[src_offs], type_size);
        } // for (uint i)
      } // for (uint c)

      fmt::print("Attempted to read: channels = {}, width = {}, height = {}, tiles = {}",
        exr_image.num_channels, exr_image.width, exr_image.height, exr_image.num_tiles);
      for (uint i = 0; i < exr_header.num_channels; ++i)
        fmt::print("\tchannel {}, named {}", i, exr_header.channels[i].name);

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
      m_size       = { w, h };

      // Allocate image memory and copy image data over
      size_t data_len = m_size.prod() * detail::size_from_format(m_pixel_frmt) 
                                      * detail::size_from_type(m_pixel_type);
      m_data = std::vector<std::byte>(ptr, ptr + data_len);

      // Cleanup
      stbi_image_free(ptr);
    }

    // If some form of conversion is requested, run it
    if (info.rgb_convert != RGBConvertType::eNone)
      *this = convert({ .rgb_convert = info.rgb_convert });
  }

  Image::Image(CreateInfo info)
  : m_pixel_frmt(info.pixel_frmt),
    m_pixel_type(info.pixel_type),
    m_size(info.size),
    m_data(m_size.prod() * detail::size_from_format(info.pixel_frmt) 
                         * detail::size_from_type(info.pixel_type)) {
    met_trace();

    // Either copy data over, or 
    // If data is provided, run a copy
    if (!info.data.empty())
      std::copy(std::execution::par_unseq, range_iter(info.data), m_data.begin());

    // If some form of conversion is requested, run it
    if (info.rgb_convert != RGBConvertType::eNone)
      *this = convert({ .rgb_convert = info.rgb_convert });
  }

  Image Image::convert(ConvertInfo info) const {
    met_trace();

    // Initialize output image in requested format
    Image output = {{ 
      .pixel_frmt = info.pixel_frmt.value_or(m_pixel_frmt),
      .pixel_type = info.pixel_type.value_or(m_pixel_type),
      .size       = m_size
    }};

    // Used sizes and offsets
    size_t inp_chan = detail::size_from_format(frmt());
    size_t out_chan = detail::size_from_format(output.frmt());
    size_t trf_chan = std::min<size_t>(inp_chan, out_chan);
    size_t inp_type_size = detail::size_from_type(type());
    size_t out_type_size = detail::size_from_type(output.type());
    size_t inp_pixl_size = inp_chan * inp_type_size;
    size_t out_pixl_size = out_chan * out_type_size;

    // Iterate over pixel values
    #pragma omp parallel for
    for (int i = 0; i < output.size().prod(); ++i) {
      size_t inp_pixel_offs = i * inp_pixl_size,
             out_pixel_offs = i * out_pixl_size;

      // Iterate over overlapping channels (all channels unless resize takes place)
      for (int j = 0; j < trf_chan; ++j) {
        size_t inp_chan_offs = inp_pixel_offs + j * inp_type_size,
               out_chan_offs = out_pixel_offs + j * out_type_size;

        // Floating point is used as a common representation for conversion
        float f = 0.f;
        
        // Convert input to common representation
        switch (type()) {
          case PixelType::eUChar:
            f = detail::convert<float>(*reinterpret_cast<const uchar *>(&m_data[inp_chan_offs])); 
            break;
          case PixelType::eUShort: 
            f = detail::convert<float>(*reinterpret_cast<const ushort *>(&m_data[inp_chan_offs]));
            break;
          case PixelType::eUInt:   
            f = detail::convert<float>(*reinterpret_cast<const uint *>(&m_data[inp_chan_offs]));
            break;
          case PixelType::eFloat:
            f = *reinterpret_cast<const float *>(&m_data[inp_chan_offs]);
            break;
        } // switch (type())

        // Apply lrgb-srgb conversion on common representation if requested
        switch (info.rgb_convert) {
         case RGBConvertType::eSRGBtoLRGB: f = lrgb_to_srgb_f(f); break;
         case RGBConvertType::eLRGBtoSRGB: f = srgb_to_lrgb_f(f); break;
        }

        // Convert common representation to output
        switch (output.type()) {
          case PixelType::eUChar:  
            *reinterpret_cast<uchar *>(&output.m_data[out_chan_offs]) = detail::convert<uchar>(f);
            break;
          case PixelType::eUShort: 
            *reinterpret_cast<ushort *>(&output.m_data[out_chan_offs]) = detail::convert<ushort>(f);
            break;
          case PixelType::eUInt:   
            *reinterpret_cast<uint *>(&output.m_data[out_chan_offs]) = detail::convert<uint>(f);
            break;
          case PixelType::eFloat:  
            *reinterpret_cast<float *>(&output.m_data[out_chan_offs]) = f;
            break;
        } // switch (output.type())
      } // for (int j)
    } // for (int i)

    return output;
  }

  uint Image::channels() const {
    return detail::size_from_format(m_pixel_frmt);
  }
} // namespace met