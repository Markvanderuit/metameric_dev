#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/utility.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  struct ViewportDrawEndTask : public detail::TaskNode {
    struct UniformBuffer {
      alignas(8) eig::Array2u size;
      alignas(4) uint lrgb_to_srgb;
    };

    gl::ComputeInfo m_dispatch;
    gl::Program     m_program;
    gl::Sampler     m_sampler;
    gl::Buffer      m_uniform_buffer;
    UniformBuffer  *m_uniform_map;

    void init(SchedulerHandle &info) override {
      met_trace_full();

      // Set up draw components for gamma correction
      m_sampler = {{ .min_filter = gl::SamplerMinFilter::eNearest, .mag_filter = gl::SamplerMagFilter::eNearest }};
      m_program = {{ .type = gl::ShaderType::eCompute, 
                     .spirv_path = "resources/shaders/misc/texture_resample.comp.spv", 
                     .cross_path = "resources/shaders/misc/texture_resample.comp.json" }};
      
      // Initialize uniform buffer and writeable, flushable mapping
      m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = gl::BufferCreateFlags::eMapWritePersistent }};
      m_uniform_map    = &m_uniform_buffer.map_as<UniformBuffer>(gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush)[0];
      m_uniform_map->lrgb_to_srgb = true;
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();
    
      // Get external resources 
      const auto &e_frame_buffer_ms = info("viewport.draw_begin", "frame_buffer_msaa").read_only<gl::Framebuffer>();
      const auto &e_lrgb_target     = info("viewport.begin", "lrgb_target").read_only<gl::Texture2d4f>();

      // Get modified resources 
      auto &e_frame_buffer = info("viewport.draw_begin", "frame_buffer").writeable<gl::Framebuffer>();

      // Blit color results into the single-sampled framebuffer with attached target draw_texture
      gl::sync::memory_barrier(gl::BarrierFlags::eFramebuffer);
      e_frame_buffer_ms.blit_to(e_frame_buffer, 
                                e_lrgb_target.size(), 0u, 
                                e_lrgb_target.size(), 0u, 
                                gl::FramebufferMaskFlags::eColor);

      // Set dispatch size correctly if input texture size changed
      if (info("viewport.begin", "lrgb_target").is_mutated()) {
        eig::Array2u dispatch_n    = e_lrgb_target.size();
        eig::Array2u dispatch_ndiv = ceil_div(dispatch_n, 16u);
        m_dispatch = { .groups_x = dispatch_ndiv.x(),
                       .groups_y = dispatch_ndiv.y(),
                       .bindable_program = &m_program };
        m_uniform_map->size = dispatch_n;
        m_uniform_buffer.flush();
      }

      // Bind image/sampler resources, then dispatch shader to perform resample/srgb conversion
      m_program.bind("b_uniform", m_uniform_buffer);
      m_program.bind("s_image_r", m_sampler);
      m_program.bind("s_image_r", e_lrgb_target);
      m_program.bind("i_image_w", info("viewport.begin", "srgb_target").writeable<gl::Texture2d4f>());
      gl::dispatch_compute(m_dispatch);
    }
  };
} // namespace met
