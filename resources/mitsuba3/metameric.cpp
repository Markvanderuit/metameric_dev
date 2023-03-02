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

// Generic string conversion routine
template <typename T> inline std::string to_string(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

/* Data block for spectral texture import format */
struct SpectralData {
  // Header data
  float spec_min;
  float spec_max;
  uint  spec_samples;
  uint  bary_xres;
  uint  bary_yres;
  uint  bary_zres;

  // Bulk data
  std::vector<float> functions;
  std::vector<float> weights;
  std::vector<float> indx;
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
    ref<ZStream>    zs = new ZStream(fs, ZStream::EStreamType::EGZipStream);

    // Read header data
    zs->read<float>(data.spec_min);
    zs->read<float>(data.spec_max);
    zs->read<unsigned>(data.spec_samples);
    zs->read<unsigned>(data.bary_xres);
    zs->read<unsigned>(data.bary_yres);
    zs->read<unsigned>(data.bary_zres);

    // Allocate weight/function data blocks, then read block data
    data.functions.resize(data.bary_zres * data.spec_samples * 4);
    data.weights.resize(data.bary_xres * data.bary_yres * 4);
    zs->read_array<float>(data.functions.data(), data.functions.size());
    zs->read_array<float>(data.weights.data(), data.weights.size());

    // Close streams
    zs->close();
    fs->close();
    
    Log(Info, "Metameric texture loaded\n");
    Log(Info, "wvl_min = %f\nwvl_max = %f\nwvl_samples = %d\nbary_xres = %d\nbary_yres = %d\nbary_zres = %d\n",
      data.spec_min, data.spec_max, data.spec_samples, 
      data.bary_xres, data.bary_yres, data.bary_zres);

    // Extract and reinterpret barycentric index data, because dr::reinterpret_array fails on the cuda backend r.n.
    data.indx.resize(data.bary_xres * data.bary_yres);
    #pragma omp parallel for
    for (int i = 0; i < data.indx.size(); ++i) {
      data.indx[i] = (float(*reinterpret_cast<const uint32_t *>(&data.weights[i * 4 + 3])) + .5f) / static_cast<float>(data.bary_zres);
      data.weights[i * 4 + 3] = 1.f - data.weights[i * 4 + 2] - data.weights[i * 4 + 1] - data.weights[i * 4];
    }

    // Wavelength data
    m_spec_sub = data.spec_min;
    m_spec_div = data.spec_max - data.spec_min;
    m_spec_size = m_spec_div / static_cast<float>(data.spec_samples);
    m_func_div = 1.f / static_cast<float>(data.bary_zres);

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
    size_t indx_shape[3] = { data.bary_yres, data.bary_xres,    1 };
    size_t bary_shape[3] = { data.bary_yres, data.bary_xres,    4 };
    size_t func_shape[3] = { data.bary_zres, data.spec_samples, 4 };

    // auto indx_tensor = dr::Tensor<mitsuba::DynamicBuffer<UInt32>>(data.indices.data(), 3, indx_shape);
    // m_indx = { indx_tensor, m_accel, m_accel, filter_mode, wrap_mode };
    m_indx = { TensorXf(data.indx.data(), 3, indx_shape), m_accel, m_accel, filter_mode, wrap_mode };
    m_bary = { TensorXf(data.weights.data(), 3, bary_shape), m_accel, m_accel, filter_mode, wrap_mode };
    m_func = { TensorXf(data.functions.data(), 3, func_shape), m_accel, m_accel, dr::FilterMode::Linear, dr::WrapMode::Clamp };

    Log(Info, "Shape is %u x %u x %u", m_bary.shape()[0], m_bary.shape()[1], m_bary.shape()[2]);
  }

  void traverse(TraversalCallback *callback) override {
    // ...
  }

  void parameters_changed(const std::vector<std::string> &keys = {}) override {
    // ...
  }
  
  UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
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

    // Output storage
    UnpolarizedSpectrum s = 0;
    if (m_bary.filter_mode() == dr::FilterMode::Linear) {
      // Fetch 4 channels of barycentric data
      Vector4f b00 = 0, b10 = 0, b01 = 0, b11 = 0;
      dr::Array<Float *, 4> fetch_bary = { b00.data(), b10.data(), b01.data(), b11.data() };
      m_bary.eval_fetch(uv, fetch_bary, active);

      // Fetch 4 channels of index data
      Float i00 = 0, i10 = 0, i01 = 0, i11 = 0;
      dr::Array<Float *, 4> fetch_indx = { &i00, &i10, &i01, &i11 };
      m_indx.eval_fetch(uv, fetch_indx, active);

      // Interpolation weights
      ScalarVector2i res = resolution();
      uv = dr::fmadd(uv, res, -.5f);
      Vector2i uv_i = dr::floor2int<Vector2i>(uv);
      Point2f w1 = uv - Point2f(uv_i),
              w0 = 1.f - w1;

      // Sample and combine wavelength data 
      auto wvls = (si.wavelengths - m_spec_sub) / m_spec_div;
      for (uint i = 0; i < 4; ++i) {
        Vector4f w00, w10, w01, w11;
        m_func.eval(Vector2f { wvls[i], i00 }, w00.data(), active);
        m_func.eval(Vector2f { wvls[i], i10 }, w10.data(), active);
        m_func.eval(Vector2f { wvls[i], i01 }, w01.data(), active);
        m_func.eval(Vector2f { wvls[i], i11 }, w11.data(), active);

        Float f0 = dr::fmadd(w0.x(), dr::dot(b00, w00), w1.x() * dr::dot(b10, w10));
        Float f1 = dr::fmadd(w0.x(), dr::dot(b01, w01), w1.x() * dr::dot(b11, w11));
        s[i]     = dr::fmadd(w0.y(), f0, w1.y() * f1);
      }
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
  Texture2f m_bary;
  Texture2f m_func;
  Texture2f m_indx;
  
  ScalarTransform3f m_transform;
  bool              m_clamp;
  bool              m_accel;
  Float             m_mean;

  std::string       m_name;
  Float             m_spec_size, m_spec_sub, m_spec_div;
  Float             m_func_div;

  // Optional: distribution for importance sampling
  mutable std::mutex m_mutex;
  std::unique_ptr<DiscreteDistribution2D<Float>> m_distr2d;
};

MI_IMPLEMENT_CLASS_VARIANT(MetamericTexture, Texture)
MI_EXPORT_PLUGIN(MetamericTexture, "Metameric texture")

NAMESPACE_END(mitsuba)