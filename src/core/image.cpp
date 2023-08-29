#include <metameric/core/image.hpp>
#include <algorithm>
#include <execution>
#include <cstdint>
#include <limits>

namespace met {
  namespace detail {
    template <typename OutTy, typename InTy>
    OutTy convert_image_value(const InTy &v) {
      return v.cast<OutTy::Scalar>().eval();
    }

    /* template <typename OutTy, typename InTy> 
    requires (std::is_same_v<InTy::Scalar, OutTy::Scalar>) 
    OutTy convert_image_value(InTy v) {
      return v;
    }

    template <typename OutTy, typename InTy>
    requires (!std::is_same_v<InTy::Scalar, OutTy::Scalar> && 
              !std::is_same_v<InTy::Scalar, float>         && 
              !std::is_same_v<OutTy::Scalar, float>)
    OutTy convert_image_value(InTy v) {
      return v
        .max(std::numeric_limits<OutTy::Scalar>::min())
        .min(std::numeric_limits<OutTy::Scalar>::max())
        .cast<OutTy::Scalar>().eval();
    }

    template <typename OutTy, typename InTy> 
    requires (!std::is_same_v<InTy::Scalar, float> && 
               std::is_same_v<OutTy::Scalar, float>) 
    OutTy convert_image_value(InTy v) {
      return v.cast<OutTy::Scalar>() / static_cast<OutTy::Scalar>(std::numeric_limits<InTy::Scalar>::max());
    }

    template <typename OutTy, typename InTy> 
    requires (std::is_same_v<InTy::Scalar, float> && 
             !std::is_same_v<OutTy::Scalar, float>) 
    OutTy convert_image_value(InTy v) {
      return (v * static_cast<InTy::Scalar>(std::numeric_limits<OutTy::Scalar>::max())).cast<OutTy::Scalar>().eval();
    } */
  } // namespace detail

  template <typename Ty> requires (is_approx_comparable<Ty>)
  Image<Ty>::Image(ImageLoadInfo info) {
    met_trace();

  }

  template <typename Ty> requires (is_approx_comparable<Ty>)
  Image<Ty>::Image(ImageCreateInfo<Ty> info) {
    met_trace();

  }
  
  template <typename InputImage, typename OutputImage>
  OutputImage convert_image(const InputImage &input, ImageConvertInfo<typename OutputImage::Type> info) 
  requires std::is_same_v<InputImage, OutputImage> {
    met_trace_n("Passthrough");

    OutputImage output = {{ .size = input.size(), .data = input.data() }};

    if (info.rgb_convert != RGBConvertType::eNone) {

    }
    
    return output;
  }

  template <typename InputImage, typename OutputImage>
  OutputImage convert_image(const InputImage &input, ImageConvertInfo<typename OutputImage::Type> info) {
    met_trace();

    constexpr auto overlap_channels = std::min<uint>(InputImage::channels(), OutputImage::channels());

    OutputImage output = {{ .size = input.size() }};

    if constexpr (std::is_same_v<InputImage, OutputImage>) {
      std::copy(std::execution::par_unseq,
                range_iter(input.data()),
                output.data().begin());
    } else {
      std::transform(std::execution::par_unseq, 
                    range_iter(input.data()), 
                    output.data().begin(), 
                    [&](const typename InputImage::Type &v) {
        typename OutputImage::Type v_(info.fill_value);
        auto v_reduced = v.head<overlap_channels>().eval();
        v_.head<overlap_channels>() = v_reduced.cast<decltype(v_)::Scalar>();
        // v_.head<overlap_channels>() = detail::convert_image_value<decltype(v_)>(v_reduced);

        // v_.head<overlap_channels>() 
        //   = detail::convert_image_value<decltype(v_), decltype(v_reduced)>(v_reduced);
        return v_;
      });
    }

    /* switch (info.rgb_convert) {
    case RGBConvertType::eLRGBtoSRGB:
      std::transform(std::execution::par_unseq,
                     range_iter(output.data()),
                     output.data().begin(),
                     [&](typename OutputImage::Type v) {
        Colr v_;

        for (uint i = 0; i < OutputImage::channels(); ++i)
          v_[i] = detail::convert_image_value<float, OutputImage::Scalar>(v[i]);

        v_ = lrgb_to_srgb(v_);

        for (uint i = 0; i < OutputImage::channels(); ++i)
          v[i] = detail::convert_image_value<OutputImage::Scalar, float>(v_[i]);

        return v;
      });
      break;
    case RGBConvertType::eSRGBtoLRGB:
      // ...
      
      break;
    }; */

    return output;
  }

/* Explicit template instantiations oh boy! Every type to every type let's go */

#define declare_image(ImageDenom)                                                                   \
  template class Image<eig::Array1   ## ImageDenom>;                                                \
  template class Image<eig::Array3   ## ImageDenom>;                                                \
  template class Image<eig::Array4   ## ImageDenom>;                                                \
  template class Image<eig::AlArray3 ## ImageDenom>;

#define declare_image_all()                                                                         \
  declare_image(f );                                                                                \
  declare_image(i );                                                                                \
  declare_image(u );                                                                                \
  declare_image(s );                                                                                \
  declare_image(us);                                                                                

#define declare_output_function(InputImage, OutputImage)                                            \
  template                                                                                          \
  OutputImage convert_image<InputImage, OutputImage>(const InputImage &, ImageConvertInfo<typename OutputImage::Type>);

#define declare_output_functions(InputImage, OutputDenom)                                           \
  declare_output_function(InputImage, Image<eig::Array1   ## OutputDenom>);                 \
  declare_output_function(InputImage, Image<eig::Array3   ## OutputDenom>);                 \
  declare_output_function(InputImage, Image<eig::Array4   ## OutputDenom>);                 \
  declare_output_function(InputImage, Image<eig::AlArray3 ## OutputDenom>);

#define declare_output_functions_all(InputImage)                                                    \
  declare_output_functions(InputImage, f );                                                         \
  declare_output_functions(InputImage, i );                                                         \
  declare_output_functions(InputImage, u );                                                         \
  declare_output_functions(InputImage, s );                                                         \
  declare_output_functions(InputImage, us);

#define declare_input_functions(OutputDenom)                                                        \
  declare_output_functions_all(Image<eig::Array1   ## OutputDenom>);                        \
  declare_output_functions_all(Image<eig::Array3   ## OutputDenom>);                        \
  declare_output_functions_all(Image<eig::Array4   ## OutputDenom>);                        \
  declare_output_functions_all(Image<eig::AlArray3 ## OutputDenom>);

#define declare_input_functions_all()                                                               \
  declare_input_functions(f );                                                                      \
  declare_input_functions(i );                                                                      \
  declare_input_functions(u );                                                                      \
  declare_input_functions(s );                                                                      \
  declare_input_functions(us);

  declare_image_all();
  declare_input_functions_all();
} // namespace met