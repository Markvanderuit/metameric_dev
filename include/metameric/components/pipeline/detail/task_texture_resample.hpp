#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/detail/trace.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met::detail {
  template <class TextureType>
  struct TextureResampleTaskCreateInfo {
    using StringPair  = std::pair<std::string, std::string>;
    using TextureInfo = TextureType::InfoType;
    using SamplerInfo = gl::Sampler::InfoType;

    StringPair  input_key;            // Key to input resource
    StringPair  output_key;           // Key to output resource (key.first is task name)
    TextureInfo texture_info = {};    // Info about output gl texture object
    SamplerInfo sampler_info = {};    // Info about internal gl sampler object
    bool        lrgb_to_srgb = false; // Perform gamma correction during resampling
  };

  template <class TextureTy>
  class TextureResampleTask : public detail::TaskBase {
  public:
    using TextureType = TextureTy;
    using InfoType    = TextureResampleTaskCreateInfo<TextureType>;
  
  private:
    InfoType        m_info;
    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
    gl::Sampler     m_sampler;

  public:
    TextureResampleTask(InfoType info)
    : m_info(info) { }
                        
    void init(SchedulerHandle &info) override {
      met_trace_full();

      // Initialize shader object
      m_program = {{ .type = gl::ShaderType::eCompute,
                     .path = "resources/shaders/misc/texture_resample.comp" }};
      m_program.uniform("u_lrgb_to_srgb", static_cast<uint>(m_info.lrgb_to_srgb));

      // Delegate remainder of initialization to set_... functions
      set_sampler_info(info, m_info.sampler_info);
      set_texture_info(info, m_info.texture_info);
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();

      guard(info.has_resource(m_info.input_key.first, m_info.input_key.second));

      // Get shared resources
      auto &e_rsrc = info.get_resource<TextureType>(m_info.input_key.first, m_info.input_key.second);
      auto &i_rsrc = info.get_resource<TextureType>(m_info.output_key.second);

      // Bind resources
      m_sampler.bind_to(0);
      e_rsrc.bind_to(gl::TextureTargetType::eTextureUnit,    0);
      i_rsrc.bind_to(gl::TextureTargetType::eImageWriteOnly, 0);
      gl::sync::memory_barrier(gl::BarrierFlags::eTextureFetch);

      // Dispatch shader, sampling texture into texture image
      gl::dispatch_compute(m_dispatch);
    }

    void set_texture_info(SchedulerHandle &info, TextureType::InfoType texture_info) {
      met_trace_full();

      m_info.texture_info = texture_info;

      // Emplace texture resource using new info object; scheduler replaces pre-existing resource
      info.emplace_resource<TextureType, TextureType::InfoType>(m_info.output_key.second, 
                                                                m_info.texture_info);

      // Compute nr. of workgroups as nearest upper divide of n / (16, 16), implying wg size of 256
      eig::Array2u dispatch_n    = m_info.texture_info.size;
      eig::Array2u dispatch_ndiv = ceil_div(dispatch_n, 16u);

      // Initialize objects for texture-to-texture resampling
      m_dispatch = { .groups_x = dispatch_ndiv.x(),
                     .groups_y = dispatch_ndiv.y(),
                     .bindable_program = &m_program };
      m_program.uniform("u_size", dispatch_n);
    }

    void set_sampler_info(SchedulerHandle &info, gl::Sampler::InfoType sampler_info) {
      met_trace_full();

      m_info.sampler_info = sampler_info;

      m_sampler = { m_info.sampler_info };
      m_program.uniform("u_sampler", 0);
    }
  };
} // namespace met::detail