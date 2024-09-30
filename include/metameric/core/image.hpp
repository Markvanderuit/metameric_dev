#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/serialization.hpp>

namespace met {
  /* Image.
     Dynamic image class which can be converted to static data when necessary. 
     Is the primary class for loading image from disk, for 
     (de)serialinzing image in the internal scene format, and for handling 
     conversion between different image types. */
  struct Image {
    enum class ColorFormat { eNone, eXYZ, eLRGB, eSRGB      }; // Supported rgb color formats
    enum class PixelFormat { eRGB, eRGBA, eAlpha            }; // Supported pixel data formats
    enum class PixelType   { eUChar, eUShort, eUInt, eFloat }; // Supported pixel data types

    struct CreateInfo {
      PixelFormat                pixel_frmt = PixelFormat::eAlpha;
      PixelType                  pixel_type = PixelType::eFloat;
      ColorFormat                color_frmt = ColorFormat::eNone;
      eig::Array2u               size;
      std::span<const std::byte> data = { };
    };

    struct LoadInfo {
      fs::path path;
    };

    struct ConvertInfo {
      eig::Array2u               resize_to  = 0;
      std::optional<PixelFormat> pixel_frmt = { };
      std::optional<PixelType>   pixel_type = { };
      std::optional<ColorFormat> color_frmt = { };
    };

  private: // Internal data
    PixelFormat            m_pixel_frmt;
    PixelType              m_pixel_type;
    ColorFormat            m_color_frmt;
    eig::Array2u           m_size;
    std::vector<std::byte> m_data;

  public: // Boilerplates
    using InfoType = CreateInfo;

    Image()  = default;
    Image(LoadInfo info);
    Image(CreateInfo info);

    bool operator==(const Image &o) const;

  public: // Pixel queries and format conversions
    void         set_pixel(const eig::Array2u &xy, eig::Array4f v, ColorFormat input_frmt = ColorFormat::eNone);
    eig::Array4f get_pixel(const eig::Array2u &xy, ColorFormat output_frmt = ColorFormat::eNone) const;
    eig::Array4f sample(const eig::Array2f &uv, ColorFormat output_frmt = ColorFormat::eNone) const;
    Image        convert(ConvertInfo info) const;
    
  public: // Misc
    auto size()       const { return m_size;       }
    uint channels()   const;
    auto pixel_frmt() const { return m_pixel_frmt; }
    auto pixel_type() const { return m_pixel_type; }
    auto color_frmt() const { return m_color_frmt; }

    template <typename Ty = std::byte>
    std::span<const Ty> data() const {
      return cnt_span<const Ty>(m_data);
    }

    template <typename Ty = std::byte>
    std::span<Ty> data() {
      return cnt_span<Ty>(m_data);
    }

    void save_exr(fs::path path) const;
    
  private: // Serialization
    void to_stream(const PixelFormat &ty, std::ostream &str) const {
      met_trace();
      str.write(reinterpret_cast<const char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
    }

    void from_stream(PixelFormat &ty, std::istream &str) {
      met_trace();
      str.read(reinterpret_cast<char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
    }

    void to_stream(const PixelType &ty, std::ostream &str) const {
      met_trace();
      str.write(reinterpret_cast<const char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
    }

    void from_stream(PixelType &ty, std::istream &str) {
      met_trace();
      str.read(reinterpret_cast<char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
    }

    void to_stream(const ColorFormat &ty, std::ostream &str) const {
      met_trace();
      str.write(reinterpret_cast<const char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
    }

    void from_stream(ColorFormat &ty, std::istream &str) {
      met_trace();
      str.read(reinterpret_cast<char *>(&ty), sizeof(std::decay_t<decltype(ty)>));
    }
    
  public: // Serialization
    void to_stream(std::ostream &str) const {
      met_trace();
      this->to_stream(m_pixel_type, str);
      this->to_stream(m_pixel_frmt, str);
      this->to_stream(m_color_frmt, str);
      io::to_stream(m_size, str);
      io::to_stream(m_data, str);
    }

    void from_stream(std::istream &str) {
      met_trace();
      this->from_stream(m_pixel_type, str);
      this->from_stream(m_pixel_frmt, str);
      this->from_stream(m_color_frmt, str);
      io::from_stream(m_size, str);
      io::from_stream(m_data, str);
    }
  };
} // namespace met