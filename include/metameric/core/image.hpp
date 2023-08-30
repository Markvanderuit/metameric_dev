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
  // FWD
  template <typename Ty> requires (is_approx_comparable<Ty>)
  struct Image;
  
  /* Declare common types */

  using Image1f = Image<eig::Array1f>;
  using Image3f = Image<eig::Array3f>;
  using Image4f = Image<eig::Array4f>;

  using Image1u = Image<eig::Array1u>;
  using Image3u = Image<eig::Array3u>;
  using Image4u = Image<eig::Array4u>;

  using Image1us = Image<eig::Array1us>;
  using Image3us = Image<eig::Array3us>;
  using Image4us = Image<eig::Array4us>;
  
  using AlImage3f  = Image<eig::AlArray3f>;
  using AlImage3u  = Image<eig::AlArray3u>;
  using AlImage3s  = Image<eig::AlArray3s>;
  using AlImage3us = Image<eig::AlArray3us>;

  /* Helper structs */

  enum class RGBConvertType {
    eNone, eSRGBtoLRGB, eLRGBtoSRGB
  };

  template <typename Ty> requires (is_approx_comparable<Ty>)
  struct ImageConvertInfo {
    // On channel resize, fill in potentially added channels as?
    Ty::Scalar     fill_value  = 0; 

    // Handle gamma conversion how?
    RGBConvertType rgb_convert = RGBConvertType::eNone;
  };

  template <typename Ty> requires (is_approx_comparable<Ty>)
  struct ImageCreateInfo {
    eig::Array2u        size;
    std::span<const Ty> data = { };
  };

  struct ImageLoadInfo {
    fs::path       path;
    RGBConvertType rgb_convert = RGBConvertType::eNone;
  };

  /* DynamicImage.
     Dynamic image class which can be converted to static image classes
     when necessary. Is the primary class for loading textures from disk, for 
     (de)serialinzing textures in the internal scene format, and for handling 
     conversion between different texture types. */
  struct DynamicImage {
    enum class PixelFormat { eRGB, eRGBA, eAlpha            }; // Supported pixel data formats
    enum class PixelType   { eUChar, eUShort, eUInt, eFloat }; // Supported pixel data types

    struct CreateInfo {
      PixelFormat                pixel_frmt;
      PixelType                  pixel_type;
      eig::Array2u               size;
      std::span<const std::byte> data = { };
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
    DynamicImage()  = default;
    ~DynamicImage() = default;
    DynamicImage(LoadInfo info);
    DynamicImage(CreateInfo info);

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

    met_declare_noncopyable(DynamicImage);

  public: // Data conversions
    DynamicImage convert(ConvertInfo info) const;

  public: // Typed and sized accessors to the underlying data
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
    
  public: // Serialization
    void to_stream(std::ostream &str) const {
      met_trace();
      io::to_stream(m_pixel_type, str);
      io::to_stream(m_pixel_frmt, str);
      io::to_stream(m_size, str);
      io::to_stream(m_data, str);
    }

    void fr_stream(std::istream &str) {
      met_trace();
      io::fr_stream(m_pixel_type, str);
      io::fr_stream(m_pixel_frmt, str);
      io::fr_stream(m_size, str);
      io::fr_stream(m_data, str);
    }
  };

  /* Helper functions */

  // Convert between image representations
  template <typename OutputImage, typename InputImage>
  OutputImage convert_image(const InputImage &image, ImageConvertInfo<typename OutputImage::Type> info = { });

  namespace detail {
    struct ImageBase {
    protected:
      eig::Array2u    m_size;

    public:
      ImageBase()  = default;
      ~ImageBase() = default;

      bool operator==(const auto &o) const {
        met_trace();
        return m_size.isApprox(o.m_size);
      }

      inline void swap(auto &o) {
        met_trace();
        std::swap(m_size, o.m_size);
      }

    public:
      virtual Image1f  realize_1f()  const = 0;
      virtual Image3f  realize_3f()  const = 0;
      virtual Image1u  realize_1u()  const = 0;
      virtual Image3u  realize_3u()  const = 0;
      virtual Image1us realize_1us() const = 0;
      virtual Image3us realize_3us() const = 0;

    public: // Type information
      auto size() const { return m_size; }
      virtual uint channels() const = 0;

    public: // Serialization
      virtual void to_stream(std::ostream &str) const = 0;
      virtual void fr_stream(std::istream &str)       = 0;
    };
  } // namespace detail

  /* Simple 2D image representation with erased channel/type conversion support */
  template <typename Ty> requires (is_approx_comparable<Ty>)
  struct Image : public detail::ImageBase {
  protected:
    std::vector<Ty> m_data;

  public:
    using Type   = Ty;
    using Scalar = Ty::Scalar;

    Image()  = default;
    ~Image() = default;

    Image(ImageLoadInfo       info);
    Image(ImageCreateInfo<Ty> info);
    
  public: // Data accessors
    constexpr const auto &data() const { return m_data; }
                    auto &data()       { return m_data; }
    constexpr       auto begin() const { return m_data.begin(); }
    constexpr       auto end()   const { return m_data.end();   }
    constexpr       auto begin()       { return m_data.begin(); }
    constexpr       auto end()         { return m_data.end();   }

  public:
    template <typename ImageTy> requires (std::is_base_of_v<detail::ImageBase, ImageTy>)
    ImageTy convert(ImageConvertInfo<typename ImageTy::Type> info) const {
      return convert_image<ImageTy>(*this, info);
    }

    virtual Image1f  realize_1f()  const override { return convert<Image1f>({});  }
    virtual Image3f  realize_3f()  const override { return convert<Image3f>({});  }
    virtual Image1u  realize_1u()  const override { return convert<Image1u>({});  }
    virtual Image3u  realize_3u()  const override { return convert<Image3u>({});  }
    virtual Image1us realize_1us() const override { return convert<Image1us>({}); }
    virtual Image3us realize_3us() const override { return convert<Image3us>({}); }

    virtual uint channels() const override { 
      return Ty::RowsAtCompileTime;
    }

    bool operator==(const auto &o) const {
      met_trace();
      return ImageBase::operator==(o) 
          && std::equal(range_iter(m_data), range_iter(o.m_data),
                        [](const auto &a, const auto &b) { return a.isApprox(b); });
    }

    inline void swap(auto &o) {
      met_trace();
      ImageBase::swap(o);
      std::swap(m_data, o.m_data);
    }

  public: // Serialization
    virtual void to_stream(std::ostream &str) const override {
      met_trace();
      io::to_stream(m_size, str);
      io::to_stream(m_data, str);
    }

    virtual void fr_stream(std::istream &str) override {
      met_trace();
      io::fr_stream(m_size, str);
      io::fr_stream(m_data, str);
    }
  };

  // Encapsulation of Image<Ty> classes using type-erasure to preserve
  // the underlying data in its loaded format until it is absolutely necessary
  // for rendering
  class ImageRef {
    std::unique_ptr<detail::ImageBase> m_image;

  public: // Structural information
    auto size()     const { return m_image->size(); }
    auto channels() const { return m_image->channels(); }
  
  public: // Type enforcement
    Image1u  realize_1u()  const { return m_image->realize_1u();  };
    Image3u  realize_3u()  const { return m_image->realize_3u();  };
    Image1us realize_1us() const { return m_image->realize_1us(); };
    Image3us realize_3us() const { return m_image->realize_3us(); };
    Image1f  realize_1f()  const { return m_image->realize_1f();  };
    Image3f  realize_3f()  const { return m_image->realize_3f();  };

  public: // Serialization
    void to_stream(std::ostream &str) const {
      met_trace();
      m_image->to_stream(str);
    }

    void fr_stream(std::istream &str) {
      met_trace();
      m_image->fr_stream(str);
    }
  };
} // namespace met