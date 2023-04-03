#include <mitsuba/core/distr_2d.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/zstream.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>
#include <drjit/tensor.h>
#include <drjit/texture.h>
#include <mutex>
#include <span>
#include <iostream>
#include <execution>
#include <algorithm>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

using uint = unsigned int;

// Expected maximum nr. of supported convex weights
constexpr uint generalized_weights = 16;

/* Data block for spectral texture import format */
struct SpectralData {
  // Resolution of single spectral function
  float spec_min;
  float spec_max;
  uint  spec_samples;
  
  // Resolution of weights data
  uint  bary_xres;
  uint  bary_yres;
  uint  bary_zres;

  // Bulk data
  std::vector<float> functions; // Spectral functions
  std::vector<float> weights;   // Convex weights
};

template <typename Float, typename Spectrum>
class MetamericGeneralizedTexture final : public Texture<Float, Spectrum> {
public:
  MI_IMPORT_TYPES(Texture)

  MetamericGeneralizedTexture(const Properties &props) : Texture(props) {
    // Load transform data
    m_transform = props.get<ScalarTransform4f>("to_uv", ScalarTransform4f()).extract();
    if (m_transform != ScalarTransform3f())
      dr::make_opaque(m_transform);

    // Resolve file path
    FileResolver* fr = Thread::thread()->file_resolver();
    fs::path file_path = fr->resolve(props.string("filename"));
    m_name = file_path.filename().string();

    // Start file data read: open file stream
    Log(Info, "Loading metameric texture from \"%s\" ..", m_name);
    SpectralData data;
    ref<FileStream> fs = new FileStream(file_path);
    ref<ZStream>    zs = new ZStream(fs, ZStream::EStreamType::EGZipStream);

    // Read header data
    zs->read<float>(data.spec_min);
    zs->read<float>(data.spec_max);
    zs->read<uint>(data.spec_samples);
    zs->read<uint>(data.bary_xres);
    zs->read<uint>(data.bary_yres);
    zs->read<uint>(data.bary_zres);

    // Allocate weight/function data blocks, then read block data
    data.functions.resize(data.bary_zres * data.spec_samples);
    data.weights.resize(generalized_weights * data.bary_yres * data.bary_xres);
    zs->read_array<float>(data.functions.data(), data.functions.size());
    zs->read_array<float>(data.weights.data(), data.weights.size());

    // Close streams
    zs->close();
    fs->close();
    
    Log(Info, "Metameric texture loaded\n");
    Log(Info, "wvl_min = %f\nwvl_max = %f\nwvl_samples = %d\nbary_xres = %d\nbary_yres = %d\nbary_zres = %d\n",
      data.spec_min, data.spec_max, data.spec_samples, 
      data.bary_xres, data.bary_yres, data.bary_zres);

    // Obtain scattered function data
    std::vector<float> func_data(generalized_weights * data.spec_samples, 0.f);
    #pragma omp parallel for
    for (int j = 0; j < data.bary_zres; ++j) {
      for (int i = 0; i < data.spec_samples; ++i) {
        func_data[i * generalized_weights + j] = data.functions[j * data.spec_samples + i];
      }
    }

    // Wavelength data
    m_spec_sub = data.spec_min;
    m_spec_div = 1.f / (data.spec_max - data.spec_min);

    // Read filter mode
    std::string filter_mode_str = props.string("filter_type", "bilinear");
    dr::FilterMode filter_mode;
    if (filter_mode_str == "nearest")
      filter_mode = dr::FilterMode::Nearest;
    else if (filter_mode_str == "bilinear")
      filter_mode = dr::FilterMode::Linear;
    else
      Throw("Invalid filter type \"%s\", must be one of: \"nearest\", or "
            "\"bilinear\"!", filter_mode_str);

    // Read wrap mode 
    std::string wrap_mode_str = props.string("wrap_mode", "repeat");
    typename dr::WrapMode wrap_mode;
    if (wrap_mode_str == "repeat")
      wrap_mode = dr::WrapMode::Repeat;
    else if (wrap_mode_str == "mirror")
      wrap_mode = dr::WrapMode::Mirror;
    else if (wrap_mode_str == "clamp")
      wrap_mode = dr::WrapMode::Clamp;
    else
      Throw("Invalid wrap mode \"%s\", must be one of: \"repeat\", "
            "\"mirror\", or \"clamp\"!", wrap_mode_str);

    // Read acceleration mode
    m_accel = props.get<bool>("accel", true);

    // Read clamping mode
    m_clamp = props.get<bool>("clamp", true);

    // Instantiate class objects
    size_t bary_shape[3] = { data.bary_yres,    data.bary_xres,   generalized_weights };
    size_t func_shape[2] = { data.spec_samples,                   generalized_weights };
    m_bary = { TensorXf(data.weights.data(), 3, bary_shape), m_accel, m_accel, filter_mode, wrap_mode };
    m_func = { TensorXf(func_data.data(), 2, func_shape), m_accel, m_accel, dr::FilterMode::Linear, dr::WrapMode::Clamp };

    Log(Info, "Shape is %u x %u x %u", m_bary.shape()[0], m_bary.shape()[1], m_bary.shape()[2]);
  }

  void traverse(TraversalCallback *callback) override {
    // ...
  }

  void parameters_changed(const std::vector<std::string> &keys = {}) override {
    // ...
  }
  
  UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
    using Bary = Vector<Float, generalized_weights>;
    
    if constexpr (!dr::is_array_v<Mask>)
      active = true;

    MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

    // Guard against unsupported rendering modes (really, only spectral is supported for a spectral texture, but anyways)
    if constexpr (!is_spectral_v<Spectrum>)
      Throw("A metameric texture was used in a non-spectral rendering pipeline!");

    // Guard against inactive evaluations
    if (dr::none_or<false>(active))
      return dr::zeros<UnpolarizedSpectrum>();

    Point2f uv = m_transform.transform_affine(si.uv);

    // Sample weights and functions for relevant UV and wavelengths
    Bary bary = 0.f;
    dr::Array<Bary, 4> funcs = { 0, 0, 0, 0 };
    auto wvls = (si.wavelengths - m_spec_sub) * m_spec_div;
    m_bary.eval(uv, bary.data(), active);
    m_func.eval(wvls[0], funcs[0].data(), active);
    m_func.eval(wvls[1], funcs[1].data(), active);
    m_func.eval(wvls[2], funcs[2].data(), active);
    m_func.eval(wvls[3], funcs[3].data(), active);

    // Assemble spectral reflectance as dot product of weights and functions
    UnpolarizedSpectrum s;
    if (m_clamp) {
      s[0] = dr::clamp(dr::dot(bary, funcs[0]), 0.f, 1.f);
      s[1] = dr::clamp(dr::dot(bary, funcs[1]), 0.f, 1.f);
      s[2] = dr::clamp(dr::dot(bary, funcs[2]), 0.f, 1.f);
      s[3] = dr::clamp(dr::dot(bary, funcs[3]), 0.f, 1.f);
    } else {
      s[0] = dr::dot(bary, funcs[0]);
      s[1] = dr::dot(bary, funcs[1]);
      s[2] = dr::dot(bary, funcs[2]);
      s[3] = dr::dot(bary, funcs[3]);
    }

    return s;
  }
  
  Float eval_1(const SurfaceInteraction3f &si, Mask active = true) const override {
    return 0.f;
  }
  
  Vector2f eval_1_grad(const SurfaceInteraction3f &si, Mask active = true) const override {
    return 0.f;
  }

  Color3f eval_3(const SurfaceInteraction3f &si, Mask active = true) const override {
    return 0.f;
  }

  std::pair<Point2f, Float> 
  sample_position(const Point2f &sample, Mask active = true) const override {
    return { 0.f, 0.f };
  }

  Float pdf_position(const Point2f &pos_, Mask active = true) const override {
    return 0.f;
  }
  
  std::pair<Wavelength, UnpolarizedSpectrum>
  sample_spectrum(const SurfaceInteraction3f &_si, const Wavelength &sample,
                  Mask active) const override {
    MI_MASKED_FUNCTION(ProfilerPhase::TextureSample, active);

    return { dr::zeros<Wavelength>(), dr::zeros<UnpolarizedSpectrum>() };
    // return { 0.f, 0.f };
  }

  ScalarVector2i resolution() const override {
    const size_t *shape = m_bary.shape();
    return { (int) shape[1], (int) shape[0] };
  }
  
  Float mean() const override { 
    return m_mean;
  }

  bool is_spatially_varying() const override { return true; }

  std::string to_string() const override {
    std::ostringstream oss;
    oss << "MetamericGeneralizedTexture[" << std::endl
        << "  name       = \"" << m_name       << "\"," << std::endl
        << "  resolution = \"" << resolution() << "\"," << std::endl
        << "  mean       = "   << m_mean       << "," << std::endl
        << "  transform  = "   << string::indent(m_transform) << std::endl
        << "]";
    return oss.str();
  }

  MI_DECLARE_CLASS()

protected:
  MI_INLINE UnpolarizedSpectrum
  interpolate_spectral(const SurfaceInteraction3f &si, Mask active) const {
    return 0.f;
  }

  MI_INLINE Float interpolate_1(const SurfaceInteraction3f &si,
                                  Mask active) const {
    return 0.f;
  }

  MI_INLINE Color3f interpolate_3(const SurfaceInteraction3f &si,
                                    Mask active) const {
    return 0.f;
  }

  void rebuild_internals(bool init_mean, bool init_distr) {
    // ...
  }

  MI_INLINE void init_distr() const {
    // ...
  }

protected:    
  Texture2f m_bary;
  Texture1f m_func;
  
  ScalarTransform3f m_transform;
  bool              m_clamp;
  bool              m_accel;
  Float             m_mean;
  std::string       m_name;
  Float             m_spec_sub, m_spec_div;

  // Optional: distribution for importance sampling
  mutable std::mutex m_mutex;
  std::unique_ptr<DiscreteDistribution2D<Float>> m_distr2d;
};

MI_IMPLEMENT_CLASS_VARIANT(MetamericGeneralizedTexture, Texture)
MI_EXPORT_PLUGIN(MetamericGeneralizedTexture, "Metameric texture (generalized)")

NAMESPACE_END(mitsuba)