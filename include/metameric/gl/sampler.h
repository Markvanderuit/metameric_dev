#pragma once

#include <metameric/gl/detail/fwd.h>
#include <metameric/gl/detail/handle.h>

namespace metameric::gl {
  /**
   * Helper data object to construct a sampler with default settings. Works 
   * with aggregate/designated initialization.
   */
  struct SamplerCreateInfo {
    SamplerMinFilter min_filter     = SamplerMinFilter::eNearest;
    SamplerMagFilter mag_filter     = SamplerMagFilter::eNearest;
    SamplerWrap wrap                = SamplerWrap::eClampToEdge;
    SamplerCompareFunc compare_func = SamplerCompareFunc::eLessOrEqual;
    SamplerCompareMode compare_mode = SamplerCompareMode::eCompare;
  };

  /**
   * Texture sampler object.
   */
  class Sampler : public Handle<> {
    using Base = Handle<>;

    SamplerMinFilter _min_filter;
    SamplerMagFilter _mag_filter;
    SamplerWrap _wrap;
    SamplerCompareFunc _compare_func;
    SamplerCompareMode _compare_mode;

  public:
    /* constr/destr */

    Sampler() = default;
    Sampler(SamplerCreateInfo info);
    ~Sampler();

    /* getters/setters */

    inline SamplerMinFilter min_filter() const { return _min_filter; }
    inline SamplerMagFilter mag_filter() const { return _mag_filter; }
    inline SamplerWrap wrap() const { return _wrap; }
    inline SamplerCompareFunc compare_func() const { return _compare_func; }
    inline SamplerCompareMode compare_mode() const { return _compare_mode; }

    void set_min_filter(SamplerMinFilter min_filter);
    void set_mag_filter(SamplerMagFilter mag_filter);
    void set_wrap(SamplerWrap wrap);
    void set_depth_compare_func(SamplerCompareFunc compare_func);
    void set_depth_compare_mode(SamplerCompareMode compare_mode);

    /* miscellaneous */

    inline constexpr void swap(Sampler &o) {
      using std::swap;
      Base::swap(o);
      swap(_min_filter, o._min_filter);
      swap(_mag_filter, o._mag_filter);
      swap(_wrap, o._wrap);
      swap(_compare_func, o._compare_func);
      swap(_compare_mode, o._compare_mode);
    }

    inline constexpr bool operator==(const Sampler &o) const {
      using std::tie;
      return Base::operator==(o)
          && std::tie(_min_filter, _mag_filter, _wrap, _compare_func, _compare_mode)
          == std::tie(o._min_filter, o._mag_filter, o._wrap, o._compare_func, o._compare_mode);
    }

    MET_NONCOPYABLE_CONSTR(Sampler);
  };

} // namespace metameric::gl