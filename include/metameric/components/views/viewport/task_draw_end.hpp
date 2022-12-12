#pragma once

#include <metameric/core/utility.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/core/detail/trace.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  struct ViewportDrawEndTask : public detail::AbstractTask {
    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
    gl::Sampler     m_sampler;

    ViewportDrawEndTask(const std::string &name)
    : detail::AbstractTask(name, true) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();

      // Set up draw components for gamma correction
      m_sampler = {{ .min_filter = gl::SamplerMinFilter::eNearest, .mag_filter = gl::SamplerMagFilter::eNearest }};
      m_program = {{ .type = gl::ShaderType::eCompute, .path = "resources/shaders/misc/texture_resample.comp" }};
      
      // Set these uniforms once
      m_program.uniform("u_sampler", 0);
      m_program.uniform("u_lrgb_to_srgb", 1u);
    }

    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
    
      // Get shared resources 
      auto &e_lrgb_target     = info.get_resource<gl::Texture2d4f>("viewport_begin", "lrgb_target");
      auto &e_srgb_target     = info.get_resource<gl::Texture2d4f>("viewport_begin", "srgb_target");
      auto &e_frame_buffer    = info.get_resource<gl::Framebuffer>("viewport_draw_begin", "frame_buffer");
      auto &e_frame_buffer_ms = info.get_resource<gl::Framebuffer>("viewport_draw_begin", "frame_buffer_msaa");

      // Blit color results into the single-sampled framebuffer with attached target draw_texture
      gl::sync::memory_barrier(gl::BarrierFlags::eFramebuffer);
      constexpr auto blit_flags = gl::FramebufferMaskFlags::eColor | gl::FramebufferMaskFlags::eDepth;
      e_frame_buffer_ms.blit_to(e_frame_buffer, e_lrgb_target.size(), 0u, e_lrgb_target.size(), 0u, blit_flags);

      // Set dispatch size correctly depending on texture size
      eig::Array2u dispatch_n    = e_srgb_target.size();
      eig::Array2u dispatch_ndiv = ceil_div(dispatch_n, 16u);
      m_dispatch = { .groups_x = dispatch_ndiv.x(),
                     .groups_y = dispatch_ndiv.y(),
                     .bindable_program = &m_program };
      m_program.uniform("u_size", dispatch_n);

      // Bind resources
      m_sampler.bind_to(0);
      e_lrgb_target.bind_to(gl::TextureTargetType::eTextureUnit,    0);
      e_srgb_target.bind_to(gl::TextureTargetType::eImageWriteOnly, 0);

      // Dispatch shader, applying gamma correction to obtain final results in draw_texture_srgb
      gl::dispatch_compute(m_dispatch);
    }
  };
} // namespace met
