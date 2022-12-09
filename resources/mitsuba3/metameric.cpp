#include <mitsuba/core/distr_2d.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/fstream.h>
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

NAMESPACE_BEGIN(mitsuba)

constexpr unsigned barycentric_weights = 8;

/* Header block for spectral texture import format */
struct SpectralDataHeader {
  float    wvl_min;
  float    wvl_max;      
  unsigned wvl_samples;
  unsigned func_count;
  unsigned wght_xres;
  unsigned wght_yres;
};

/* Data block for spectral texture import format */
struct SpectralData {
  SpectralDataHeader header;
  std::vector<float> functions;
  std::vector<float> weights;
};

template <typename Float, typename Spectrum>
class MetamericTexture final : public Texture<Float, Spectrum> {
public:
  MI_IMPORT_TYPES(Texture)

  MetamericTexture(const Properties &props) : Texture(props) {
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

    // Read header data
    fs->read<float>(data.header.wvl_min);
    fs->read<float>(data.header.wvl_max);
    fs->read<unsigned>(data.header.wvl_samples);
    fs->read<unsigned>(data.header.func_count);
    fs->read<unsigned>(data.header.wght_xres);
    fs->read<unsigned>(data.header.wght_yres);

    Log(Info, "Metameric texture header data loaded\n");
    Log(Info, "wvl_min = %f\nwvl_max = %f\nwvl_samples = %d\nfunc_count = %d\nwght_xres = %d\nwght_yres = %d\n",
      data.header.wvl_min, data.header.wvl_max, data.header.wvl_samples, 
      data.header.func_count, data.header.wght_xres, data.header.wght_yres);

    // Allocate weight/function data blocks
    data.functions.resize(data.header.func_count * data.header.wvl_samples);
    data.weights.resize(data.header.func_count * data.header.wght_xres * data.header.wght_yres);

    // Read block data
    fs->read_array<float>(data.functions.data(), data.functions.size());
    fs->read_array<float>(data.weights.data(), data.weights.size());

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

    // Instantiate class objects
    size_t wght_shape[3] = { data.header.wght_yres, data.header.wght_xres, data.header.func_count };
    size_t func_shape[2] = { data.header.wvl_samples, data.header.func_count };
    m_wght = { TensorXf(data.weights.data(), 3, wght_shape), 
      m_accel, m_accel, filter_mode, wrap_mode };
    m_func = { TensorXf(data.functions.data(), 2, func_shape), 
      m_accel, m_accel, dr::FilterMode::Linear, dr::WrapMode::Clamp };
  }

  void traverse(TraversalCallback *callback) override {
    // ...
  }

  void parameters_changed(const std::vector<std::string> &keys = {}) override {
    // ...
  }
  
  UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
    using Weight = Vector<Float, barycentric_weights>;
    
    if constexpr (!dr::is_array_v<Mask>)
      active = true;

    MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

    // Guard against unsupported rendering modes (really, only spectral is supported)
    if constexpr (!is_spectral_v<Spectrum>)
      Throw("A metameric texture was used in a non-spectral rendering pipeline!");

    // Guard against inactive evaluations
    if (dr::none_or<false>(active))
      return dr::zeros<UnpolarizedSpectrum>();

    Point2f uv = m_transform.transform_affine(si.uv);

    Weight wght = 0.f;
    dr::Array<Weight, 4> funcs = { 0, 0, 0, 0 };
    if (m_accel) {
      m_wght.eval(uv, wght.data(), active);
      m_func.eval((si.wavelengths[0] - MI_CIE_MIN) / (MI_CIE_MAX - MI_CIE_MIN), 
                  funcs[0].data(), active);
      m_func.eval((si.wavelengths[1] - MI_CIE_MIN) / (MI_CIE_MAX - MI_CIE_MIN), 
                  funcs[1].data(), active);
      m_func.eval((si.wavelengths[2] - MI_CIE_MIN) / (MI_CIE_MAX - MI_CIE_MIN), 
                  funcs[2].data(), active);
      m_func.eval((si.wavelengths[3] - MI_CIE_MIN) / (MI_CIE_MAX - MI_CIE_MIN), 
                  funcs[3].data(), active);
    } else {
      m_wght.eval_nonaccel(uv, wght.data(), active);
      for (unsigned i = 0; i < 4; ++i) {
        auto wvl_uv = (si.wavelengths[i] - MI_CIE_MIN) / (MI_CIE_MAX - MI_CIE_MIN);
        m_func.eval_nonaccel(wvl_uv, funcs[i].data(), active);
      }
    }

    Spectrum s;
    s[0] = dr::dot(wght, funcs[0]); 
    s[1] = dr::dot(wght, funcs[1]); 
    s[2] = dr::dot(wght, funcs[2]); 
    s[3] = dr::dot(wght, funcs[3]); 
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

    return { 0.f, 0.f };
  }

  ScalarVector2i resolution() const override {
    const size_t *shape = m_wght.shape();
    return { (int) shape[1], (int) shape[0] };
  }
  
  Float mean() const override { 
    return m_mean;
  }

  bool is_spatially_varying() const override { return true; }

  std::string to_string() const override {
    std::ostringstream oss;
    oss << "MetamericTexture[" << std::endl
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
  Texture2f         m_wght;
  Texture1f         m_func;
  ScalarTransform3f m_transform;
  bool              m_accel;
  Float             m_mean;
  std::string       m_name;

  // Optional: distribution for importance sampling
  mutable std::mutex m_mutex;
  std::unique_ptr<DiscreteDistribution2D<Float>> m_distr2d;
};

MI_IMPLEMENT_CLASS_VARIANT(MetamericTexture, Texture)
MI_EXPORT_PLUGIN(MetamericTexture, "Metameric texture")

NAMESPACE_END(mitsuba)