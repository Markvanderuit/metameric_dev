#pragma once

#include <metameric/core/io.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/serialization.hpp>
#include <metameric/core/spectrum.hpp>
#include <vector>

namespace met {
  enum class RGBConvertType {
    eNone,
    eSRGBtoLRGB,
    eLRGBtoSRGB
  };

  template <typename Ty> requires (is_approx_comparable<Ty>)
  struct ImageConvertInfo {
    RGBConvertType rgb_convert = RGBConvertType::eNone;
    Ty::Scalar     fill_value  = 0; // On channel resize
  };

  template <typename Ty> requires (is_approx_comparable<Ty>)
  struct ImageCreateInfo {
    eig::Array2u        size;
    std::span<const Ty> data = { };
    RGBConvertType      rgb_convert = RGBConvertType::eNone;
  };

  struct ImageLoadInfo {
    fs::path       path;
    RGBConvertType rgb_convert = RGBConvertType::eNone;
  };

  namespace detail {
    struct ImageBase {
    protected:
      eig::Array2u    m_size;

    public:
      ImageBase()  = default;
      ~ImageBase() = default;
      
      auto size() const { return m_size; }

      bool operator==(const auto &o) const {
        met_trace();
        return m_size.isApprox(o.m_size);
      }

      inline void swap(auto &o) {
        met_trace();
        std::swap(m_size, o.m_size);
      }

    public: // Serialization
      virtual void to_stream(std::ostream &str) const = 0;
      virtual void fr_stream(std::istream &str)       = 0;
    };
  } // namespace detail

  /* Simple 2D image representation with channel/type conversion support */
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

    
    const auto &data() const { return m_data; }
          auto &data()       { return m_data; }

  public:
    constexpr static auto channels() { return Ty::RowsAtCompileTime; }

    bool operator==(const auto &o) const {
      met_trace();
      return ImageBase::operator==(o) 
          && std::equal(range_iter(m_data), 
                        range_iter(o.m_data),
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

  // Convert between image representations
  template <typename InputImage, typename OutputImage>
  OutputImage convert_image(const InputImage &image, ImageConvertInfo<typename OutputImage::Type> info = { });

  /* Declare common types */

  using Image1f = Image<eig::Array1f>;
  using Image3f = Image<eig::Array3f>;
  using Image4f = Image<eig::Array4f>;

  using Image1u = Image<eig::Array1u>;
  using Image3u = Image<eig::Array3u>;
  using Image4u = Image<eig::Array4u>;

  using Image1s = Image<eig::Array1s>;
  using Image3s = Image<eig::Array3s>;
  using Image4s = Image<eig::Array4s>;

  using Image1us = Image<eig::Array1us>;
  using Image3us = Image<eig::Array3us>;
  using Image4us = Image<eig::Array4us>;
  
  using AlImage3f  = Image<eig::AlArray3f>;
  using AlImage3u  = Image<eig::AlArray3u>;
  using AlImage3s  = Image<eig::AlArray3s>;
  using AlImage3us = Image<eig::AlArray3us>;
} // namespace met