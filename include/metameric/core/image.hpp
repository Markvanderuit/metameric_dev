#pragma once

#include <metameric/core/io.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/serialization.hpp>
#include <metameric/core/spectrum.hpp>
#include <algorithm>
#include <execution>
#include <memory>
#include <optional>
#include <vector>

namespace met {
  enum class RGBConvertType {
    eNone, eSRGBtoLRGB, eLRGBtoSRGB
  };

  /* Image.
     Dynamic image class which can be converted to static data when necessary. 
     Is the primary class for loading image from disk, for 
     (de)serialinzing image in the internal scene format, and for handling 
     conversion between different image types. */
  struct Image {
    enum class PixelFormat { eRGB, eRGBA, eAlpha            }; // Supported pixel data formats
    enum class PixelType   { eUChar, eUShort, eUInt, eFloat }; // Supported pixel data types

    struct CreateInfo {
      PixelFormat                pixel_frmt;
      PixelType                  pixel_type;
      eig::Array2u               size;
      std::span<const std::byte> data        = { };
      RGBConvertType             rgb_convert = RGBConvertType::eNone;
    };

    struct LoadInfo {
      fs::path       path;
      RGBConvertType rgb_convert = RGBConvertType::eNone;
    };

    struct ConvertInfo {
      std::optional<PixelFormat> pixel_frmt = { };
      std::optional<PixelType>   pixel_type = { };
      RGBConvertType             rgb_convert = RGBConvertType::eNone;
    };

  private: // Internal data
    PixelFormat            m_pixel_frmt;
    PixelType              m_pixel_type;
    eig::Array2u           m_size;
    std::vector<std::byte> m_data;

  public: // Boilerplates
    using InfoType = CreateInfo;

    Image()  = default;
    ~Image() = default;
    Image(LoadInfo info);
    Image(CreateInfo info);

    inline
    void swap(auto &o) {
      met_trace();
      using std::swap;
      swap(m_pixel_frmt, o.m_pixel_frmt);
      swap(m_pixel_type, o.m_pixel_type);
      swap(m_size, o.m_size);
      swap(m_data, o.m_data);
    }

    inline
    bool operator==(const auto &o) const {
      return std::tie(m_pixel_frmt, m_pixel_type) 
          == std::tie(o.m_pixel_frmt, o.m_pixel_type)
          && m_size.isApprox(o.m_size)
          && std::equal(std::execution::par_unseq, 
                        range_iter(m_data), range_iter(o.m_data));
    }

    met_declare_noncopyable(Image);

  public: // Misc
    Image convert(ConvertInfo info) const;

    // Helper, though channel count isn't stored
    uint channels() const;

    auto frmt() const { return m_pixel_frmt; }
    auto type() const { return m_pixel_type; }
    auto size() const { return m_size;       }

    template <typename Ty = std::byte>
    std::span<const Ty> data() const {
      return cnt_span<const Ty>(m_data);
    }

    template <typename Ty = std::byte>
    std::span<Ty> data() {
      return cnt_span<Ty>(m_data);
    }
  
  public: // Conversion to static formats
    
  private: // Serialization
    void to_stream(const PixelFormat &ty, std::ostream &str) const {
      met_trace();
      str.write(reinterpret_cast<const char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
    }

    void fr_stream(PixelFormat &ty, std::istream &str) {
      met_trace();
      str.read(reinterpret_cast<char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
    }

    void to_stream(const PixelType &ty, std::ostream &str) const {
      met_trace();
      str.write(reinterpret_cast<const char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
    }

    void fr_stream(PixelType &ty, std::istream &str) {
      met_trace();
      str.read(reinterpret_cast<char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
    }
    
    void to_stream(std::ostream &str) const {
      met_trace();
      this->to_stream(m_pixel_type, str);
      this->to_stream(m_pixel_frmt, str);
      io::to_stream(m_size, str);
      io::to_stream(m_data, str);
    }

    void fr_stream(std::istream &str) {
      met_trace();
      this->fr_stream(m_pixel_type, str);
      this->fr_stream(m_pixel_frmt, str);
      io::fr_stream(m_size, str);
      io::fr_stream(m_data, str);
    }
  };
} // namespace met