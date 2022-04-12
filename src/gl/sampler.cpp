#include <metameric/gl/sampler.h>
#include <glad/glad.h>

namespace metameric::gl {
  Sampler::Sampler(SamplerMinFilter min_filter,
                   SamplerMagFilter mag_filter,
                   SamplerWrap wrap,
                   SamplerCompareFunc compare_func,
                   SamplerCompareMode compare_mode)
  : Handle<>(true), _min_filter(min_filter), _mag_filter(mag_filter), 
      _wrap(wrap), _compare_func(compare_func), _compare_mode(compare_mode)
    {
    if (!_is_init) {
      return;
    }
    
    glCreateSamplers(1, &_object);

    glSamplerParameteri(_object, GL_TEXTURE_MIN_FILTER, (uint) min_filter);
    glSamplerParameteri(_object, GL_TEXTURE_MAG_FILTER, (uint) mag_filter);
    glSamplerParameteri(_object, GL_TEXTURE_WRAP_R, (uint) wrap);
    glSamplerParameteri(_object, GL_TEXTURE_WRAP_S, (uint) wrap);
    glSamplerParameteri(_object, GL_TEXTURE_WRAP_T, (uint) wrap);
    glSamplerParameteri(_object, GL_TEXTURE_COMPARE_FUNC, (uint) compare_func);
    glSamplerParameteri(_object, GL_TEXTURE_COMPARE_MODE, (uint) compare_mode);
  }

  
  Sampler::Sampler(SamplerCreateInfo info)
  : Sampler(info.min_filter, info.mag_filter, info.wrap, info.compare_func, info.compare_mode) { }

  Sampler::~Sampler() {
    if (!_is_init) {
      return;
    }
    glDeleteSamplers(1, &_object);
  }

  void Sampler::set_min_filter(SamplerMinFilter min_filter) {
    _min_filter = min_filter;
    glSamplerParameteri(_object, GL_TEXTURE_MIN_FILTER, (uint) min_filter);
  }

  void Sampler::set_mag_filter(SamplerMagFilter mag_filter) {
    _mag_filter = mag_filter;
    glSamplerParameteri(_object, GL_TEXTURE_MAG_FILTER, (uint) mag_filter);
  }

  void Sampler::set_wrap(SamplerWrap wrap) {
    _wrap = wrap;
    glSamplerParameteri(_object, GL_TEXTURE_WRAP_R, (uint) wrap);
    glSamplerParameteri(_object, GL_TEXTURE_WRAP_S, (uint) wrap);
    glSamplerParameteri(_object, GL_TEXTURE_WRAP_T, (uint) wrap);
  }

  void Sampler::set_depth_compare_func(SamplerCompareFunc compare_func) {
    _compare_func = compare_func;
    glSamplerParameteri(_object, GL_TEXTURE_COMPARE_FUNC, (uint) compare_func);
  }

  void Sampler::set_depth_compare_mode(SamplerCompareMode compare_mode) {
    _compare_mode = compare_mode;
    glSamplerParameteri(_object, GL_TEXTURE_COMPARE_MODE, (uint) compare_mode);
  }
} // namespace metameric::gl