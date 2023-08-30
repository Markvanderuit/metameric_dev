#include <metameric/core/image.hpp>
#include <algorithm>
#include <execution>
#include <cstdint>
#include <limits>
#include <functional>

namespace met {
  namespace detail {
    // Convert image values; pass-through if values are identical
    template <typename OArr, typename IArr> 
    requires (std::is_same_v<IArr, OArr>) 
    OArr convert_value_impl(IArr v) {
      return v;
    }
    
    // Convert image values; handle aligned-unaligned float conversion
    template <typename OArr, typename IArr, typename OTy = OArr::Scalar, typename ITy = IArr::Scalar> 
    requires (!std::is_same_v<IArr, OArr> && std::is_floating_point_v<ITy> && std::is_floating_point_v<OTy>)
    OArr convert_value_impl(IArr v) {
      return v;
    }

    // Convert image values; handle clipped integer type conversion
    template <typename OArr, typename IArr, typename OTy = OArr::Scalar, typename ITy = IArr::Scalar> 
    requires (!std::is_same_v<IArr, OArr> && std::is_integral_v<ITy> && std::is_integral_v<OTy>)
    OArr convert_value_impl(IArr v) {
      return v.max(std::numeric_limits<OTy>::min()).min(std::numeric_limits<OTy>::max()).cast<OTy>().eval();
    }

    // Convert image values; handle integer type to float type converison
    template <typename OArr, typename IArr, typename OTy = OArr::Scalar, typename ITy = IArr::Scalar> 
    requires (std::is_integral_v<ITy> && std::is_floating_point_v<OTy>) 
    OArr convert_value_impl(IArr v) {
      constexpr auto f_div = static_cast<OTy>(std::numeric_limits<ITy>::max());
      return (v.cast<OTy>() / f_div).eval();
    }

    // Convert image values; handle float type to integer type converison
    template <typename OArr, typename IArr, typename OTy = OArr::Scalar, typename ITy = IArr::Scalar> 
    requires (std::is_floating_point_v<ITy> && std::is_integral_v<OTy>) 
    OArr convert_value_impl(IArr v) {
      constexpr auto f_mul = static_cast<ITy>(std::numeric_limits<OTy>::max());
      return (v * f_mul).cast<OTy>().eval();
    }
    
    template <typename OArr, typename IArr>
    concept is_same_channels = (OArr::RowsAtCompileTime == IArr::RowsAtCompileTime);

    template <typename OArr, typename IArr, typename OTy = OArr::Scalar>
    OArr convert_value(IArr v, OTy fill_value = 0) {
      if constexpr (is_same_channels<IArr, OArr>) {
        return convert_value_impl<OArr>(v);
      } else {
        constexpr auto C = std::min<uint>(IArr::RowsAtCompileTime, OArr::RowsAtCompileTime);
        auto intrm = convert_value_impl<eig::Array<OTy, C, 1>>(v.head<C>().eval());
        return (OArr(fill_value) << intrm).finished();
      }
    }
  } // namespace detail

  template <typename InputImage, typename OutputImage>
  OutputImage convert_image(const InputImage &input, ImageConvertInfo<typename OutputImage::Type> info) {
    met_trace();

    using namespace std::placeholders;
    using InputType  = typename InputImage::Type;
    using OutputType = typename OutputImage::Type;

    // Declare output image of identical dimensions, but not necessarily channels
    OutputImage output = {{ .size = input.size() }};

    if constexpr (std::is_same_v<OutputType, InputType>) {
      // If type is identical, do a direct data copy
      std::copy(std::execution::par_unseq, range_iter(input), output.begin());
    } else {
      // If type or nr of channels differs, handle type conversion gracefully
      auto f = std::bind(detail::convert_value<OutputType, InputType>, _1, info.fill_value);
      std::transform(std::execution::par_unseq, range_iter(input), output.begin(), f);
    }

    // If requested, apply or strip sRGB gamma correction; first uplift type to full float
    // color for conversion, however
    if (info.rgb_convert != RGBConvertType::eNone) {
      std::transform(std::execution::par_unseq, range_iter(output), output.begin(), [&](const auto &v) {
        Colr intrm = detail::convert_value<Colr>(v, 0.f);
        switch (info.rgb_convert) {
          case RGBConvertType::eLRGBtoSRGB: intrm = lrgb_to_srgb(intrm); break;
          case RGBConvertType::eSRGBtoLRGB: intrm = srgb_to_lrgb(intrm); break;
        };
        return detail::convert_value<OutputType>(intrm);
      });
    }

    return output;
  }

  /* Explicit template instantiations */

#define declare_output_function(InputImage, OutputImage)                                            \
  template                                                                                          \
  OutputImage convert_image<InputImage, OutputImage>(const InputImage &, ImageConvertInfo<typename OutputImage::Type>);

#define declare_output_functions(InputImage, OutputDenom)                                           \
  declare_output_function(InputImage, Image<eig::Array1   ## OutputDenom>)                          \
  declare_output_function(InputImage, Image<eig::Array3   ## OutputDenom>)                          \
  declare_output_function(InputImage, Image<eig::Array4   ## OutputDenom>)                          \
  declare_output_function(InputImage, Image<eig::AlArray3 ## OutputDenom>) 

#define declare_output_functions_all(InputImage)                                                    \
  declare_output_functions(InputImage, f )                                                          \
  declare_output_functions(InputImage, u )                                                          \
  declare_output_functions(InputImage, s )                                                          \
  declare_output_functions(InputImage, us) 

#define declare_input_functions(OutputDenom)                                                        \
  declare_output_functions_all(Image<eig::Array1   ## OutputDenom>)                                 \
  declare_output_functions_all(Image<eig::Array3   ## OutputDenom>)                                 \
  declare_output_functions_all(Image<eig::Array4   ## OutputDenom>)                                 \
  declare_output_functions_all(Image<eig::AlArray3 ## OutputDenom>) 

  declare_input_functions(f )
  declare_input_functions(u )
  declare_input_functions(s )
  declare_input_functions(us)
} // namespace met