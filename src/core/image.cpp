#include <metameric/core/image.hpp>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace met {
  namespace detail {
    template <typename InTy, typename OutTy>
    requires (!std::is_same_v<InTy, float> && !std::is_same_v<OutTy, float>)
    OutTy convert_image_value(InTy v) {
      return std::clamp(v, std::numeric_limits<OutTy>::min(), std::numeric_limits<OutTy>::max());
    }

    template <typename InTy, typename OutTy> 
    requires (!std::is_same_v<InTy, float> && std::is_same_v<OutTy, float>) 
    OutTy convert_image_value(InTy v) {
      return static_cast<OutTy>(v) / static_cast<OutTy>(std::numeric_limits<InTy>::max());
    }
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

    OutputImage output = {{ .size = input.size() }};

    if (info.rgb_convert != RGBConvertType::eNone) {

    }
    
    return output;
  }

/* Explicit template instantiations oh boy! Every type to every type let's go */

#define declare_image(ImageDenom)                                                                   \
  template class Image<eig::Array1   ## ImageDenom>;                                                \
  template class Image<eig::Array2   ## ImageDenom>;                                                \
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
  declare_output_function(InputImage, Image<eig::Array2   ## OutputDenom>);                 \
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
  declare_output_functions_all(Image<eig::Array2   ## OutputDenom>);                        \
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