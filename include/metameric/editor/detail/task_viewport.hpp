// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/editor/detail/imgui.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/renderbuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met::detail {
  // Helper object for creating viewport begin/image/end tasks
  struct ViewportTaskInfo {
    std::string  name         = "Viewport"; // Surrounding window name
    eig::Array2u size         = { -1, -1 }; // Default initial window size
    bool         is_closeable = false;      // Whether a close button appears, killing parent task on close
    bool         apply_srgb   = true;       // Whether draw output is converted in lrgb-srgb resample
  };

  // Helper task to set up a viewport; followed by ViewportImageTask and ViewportEndTask.
  // Instantiates imgui viewport
  class ViewportBeginTask : public detail::TaskNode {
    ViewportTaskInfo m_info;

  public:
    ViewportBeginTask(ViewportTaskInfo info) : m_info(info) { met_trace(); }
    
    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Keep scoped ImGui state around s.t. image can fill window
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};

      // Define window size on first open
      ImGui::SetNextWindowSize(m_info.size.cast<float>().eval(), ImGuiCond_Appearing);

      // Open main viewport window, and forward window activity to "is_active" flag
      // Note: window end is post-pended in ViewportEndTask so subtasks can do stuff with imgui state
      // Note: we track close button as an edge case
      bool is_open = true;
      bool is_active = info.parent()("is_active").getw<bool>() 
                     = ImGui::Begin(m_info.name.c_str(), m_info.is_closeable ? &is_open : nullptr);
      
      // Close prematurely; subsequent tasks should not activate either way
      if (!is_active || !is_open)
        ImGui::End();
      
      // Close button pressed; ensure related tasks get torn down gracefully
      // and close ImGui scope prematurely
      if (!is_open) {
        info.parent()("is_active").set(false);
        info.parent_task().dstr();
        return; 
      }
    }
  };

  // Helper task to set up a viewport; manages linear and srgb image targets,
  // and forwards the srgb target to instantiated imgui viewport
  class ViewportImageTask : public detail::TaskNode {
    using Depthbuffer = gl::Renderbuffer<gl::DepthComponent, 1>;

    // Constructor info
    ViewportTaskInfo m_info;

    // GL objects
        Depthbuffer  m_depthbuffer;
    gl::Framebuffer  m_framebuffer;

  private:
    void resize_fb(SchedulerHandle &info, eig::Array2u size) {
      met_trace_full();

      // Get shared resources
      auto &i_lrgb_target  = info("lrgb_target").getw<gl::Texture2d4f>();
      auto &i_srgb_target  = info("srgb_target").getw<gl::Texture2d4f>();
      
      // Recreate texture resources
      i_lrgb_target  = {{ .size = size }};
      i_srgb_target  = {{ .size = size }};
      m_depthbuffer = {{ .size = size }};

      // Recreate framebuffer, bound to newly resized resources
      m_framebuffer = {{ .type = gl::FramebufferType::eColor, .attachment = &i_lrgb_target },
                       { .type = gl::FramebufferType::eDepth, .attachment = &m_depthbuffer }};
    }

  public:
    ViewportImageTask(ViewportTaskInfo info) : m_info(info) { met_trace(); }

    bool is_active(SchedulerHandle &info) override {
      met_trace();
      return info.parent()("is_active").getr<bool>();
    }

    void init(SchedulerHandle &info) override {
      met_trace();

      info("lrgb_target").init<gl::Texture2d4f>({ .size = 1 });
      info("srgb_target").init<gl::Texture2d4f>({ .size = 1 });
      
      resize_fb(info, { 1, 1 });
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();
      
      // Get shared resources
      const auto &i_srgb_target = info("srgb_target").getr<gl::Texture2d4f>();

      // Keep scoped ImGui state around s.t. image can fill window
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};
                          
      ImGui::BeginChild("##viewport_image_view");

      // Compute viewport size s.t. texture fills rest of window
      // and if necessary resize framebuffer
      eig::Array2u image_size = static_cast<eig::Array2f>(ImGui::GetContentRegionAvail()).max(1.f).cast<uint>();
      if ((i_srgb_target.size() != image_size).any()) {
        resize_fb(info, image_size);
      }

      // Prepare framebuffer target for potential draw tasks
      m_framebuffer.bind();
      m_framebuffer.clear(gl::FramebufferType::eColor, eig::Array4f(0, 0, 0, 0));
      m_framebuffer.clear(gl::FramebufferType::eDepth, 1.f);

      // Place texture view using draw target
      ImGui::Image(i_srgb_target.object(), 
        i_srgb_target.size().cast<float>().eval(), 
        eig::Vector2f(0, 1), eig::Vector2f(1, 0));
    }
  };

  // Helper task to set up a viewport; dispatches transform from the user-accessible linear
  // to the shown srgb image target
  class ViewportEndTask : public detail::TaskNode {
    struct UniformBuffer {
      alignas(8) eig::Array2u size;
      alignas(4) uint lrgb_to_srgb;
    };

    // Constructor info
    ViewportTaskInfo m_info;

    // GL objects
    std::string     m_program_key;
    gl::ComputeInfo m_dispatch;
    gl::Sampler     m_sampler;
    gl::Buffer      m_uniform_buffer;
    UniformBuffer  *m_uniform_map;

  public:
    ViewportEndTask(ViewportTaskInfo info) : m_info(info) { met_trace(); }

    bool is_active(SchedulerHandle &info) override {
      return info.parent()("is_active").getr<bool>();
    }

    void init(SchedulerHandle &info) override {
      met_trace_full();
    
      // Initialize program object in cache
      std::tie(m_program_key, std::ignore) = info.global("cache").getw<gl::ProgramCache>().set({{ 
        .type       = gl::ShaderType::eCompute,
        .glsl_path  = "shaders/editor/detail/texture_resample.comp",
        .spirv_path = "shaders/editor/detail/texture_resample.comp.spv",
        .cross_path = "shaders/editor/detail/texture_resample.comp.json"
      }});
      
      // NN-sampler
      m_sampler = {{ .min_filter = gl::SamplerMinFilter::eNearest, 
                     .mag_filter = gl::SamplerMagFilter::eNearest }};
      
      // Initialize uniform buffer and writeable, flushable mapping
      std::tie(m_uniform_buffer, m_uniform_map) = gl::Buffer::make_flusheable_object<UniformBuffer>();
      m_uniform_map->lrgb_to_srgb = true;
    }

    void eval(SchedulerHandle &info) override {
      met_trace_full();

      // Keep scoped ImGui state around s.t. image can fill window
      auto imgui_state = { ImGui::ScopedStyleVar(ImGuiStyleVar_WindowRounding, 16.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f), 
                           ImGui::ScopedStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f })};

      if (m_info.apply_srgb) {
        // Get shared resources
        auto image_handle         = info.relative("viewport_image");
        const auto &e_lrgb_target = image_handle("lrgb_target").getr<gl::Texture2d4f>();
        const auto &e_srgb_target = image_handle("srgb_target").getr<gl::Texture2d4f>();

        // Push new dispatch size, if associated textures were modified
        if (image_handle("lrgb_target").is_mutated() || is_first_eval()) {
          eig::Array2u dispatch_n    = e_lrgb_target.size();
          eig::Array2u dispatch_ndiv = ceil_div(dispatch_n, 16u);
          m_dispatch = { .groups_x = dispatch_ndiv.x(), .groups_y = dispatch_ndiv.y()};
          m_uniform_map->size = dispatch_n;
          m_uniform_buffer.flush();
        }

        // Draw relevant program from cache
        auto &program = info.global("cache").getw<gl::ProgramCache>().at(m_program_key);

        // Bind image/sampler resources and program
        program.bind();
        program.bind("b_uniform", m_uniform_buffer);
        program.bind("s_image_r", m_sampler);
        program.bind("s_image_r", e_lrgb_target);
        program.bind("i_image_w", e_srgb_target);

        // Dispatch lrgb->srgb conversion
        gl::dispatch_compute(m_dispatch);
      } else {
        // Get shared resources
        auto image_handle         = info.relative("viewport_image");
        const auto &e_lrgb_target = image_handle("lrgb_target").getr<gl::Texture2d4f>();
        auto &e_srgb_target       = image_handle("srgb_target").getw<gl::Texture2d4f>();

        // Manually copy over so both targets are matching
        e_lrgb_target.copy_to(e_srgb_target);
      }
      
      // Switch back to default framebuffer
      gl::Framebuffer::make_default().bind();
      
      // Close child separator zone and finish ImGui State
      // Note: window end is post-pended here, but window begin is in ViewportBeginTask
      ImGui::EndChild();
      ImGui::End();
    }
  };
} // namespace met::detail