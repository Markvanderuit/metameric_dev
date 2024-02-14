#pragma once

#include <metameric/core/scene.hpp>
#include <metameric/core/scheduler.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/window.hpp>

namespace met::detail {
  class ArcballInputTask : public detail::TaskNode {
    using InfoType = detail::ArcballCreateInfo;
    
    InfoType       m_info;
    ResourceHandle m_view_handle;

  public:
    // Constructor defaults to sensible arcball settings
    // - prior; arcb_handle to corresponding target viewport; should hold gl::Texture2d4f
    // - info; arcball initialization settings
    ArcballInputTask(ResourceHandle view, InfoType info = {
      .dist            = 2.f,
      .e_eye           = { -.5f, .5f, 1.f },
      .e_center        = { -.5f, .5f, .0f },
      .zoom_delta_mult = 0.1f
    }) : m_view_handle(view), m_info(info) { }

    void init(SchedulerHandle &info) override {
      met_trace();

      m_view_handle.reinitialize(info);
      debug::check_expr(m_view_handle.is_init());

      // Get shared resources
      const auto &e_view = m_view_handle.getr<gl::Texture2d4f>();
      auto view_size = e_view.size().cast<float>().eval();

      // Make arcball available as "arcball" resource 
      auto &i_arcball = info("arcball").init<Arcball>(m_info).getw<Arcball>();
      
      // Initialize arcball properties to match the viewport
      i_arcball.set_aspect(view_size.x() / view_size.y());
    }

    void eval(SchedulerHandle &info) override {
      met_trace();
      
      // Get relevant handles and resources
      auto arcb_handle   = info("arcball");
      const auto &io     = ImGui::GetIO();
      const auto &e_view = m_view_handle.getr<gl::Texture2d4f>();

      // Get float representation of view size
      auto view_size = e_view.size().cast<float>().eval();
      
      // On viewport change, arcb_handle aspect ratio      
      if (m_view_handle.is_mutated()) {
        arcb_handle.getw<detail::Arcball>()
                   .set_aspect(view_size.x() / view_size.y());
      }
      
      // If enclosing viewport is not hovered, 
      // exit now instead of handling user input
      guard(ImGui::IsItemHovered());

      // Handle mouse scroll
      if (io.MouseWheel != 0.f) {
        arcb_handle.getw<detail::Arcball>()
              .set_zoom_delta(-io.MouseWheel);
      }

      // Handle right mouse controll
      if (io.MouseDown[1]) {
        arcb_handle.getw<detail::Arcball>()
              .set_ball_delta(eig::Array2f(io.MouseDelta) / view_size.array());
      }

      // Handle middle mouse controll
      if (io.MouseDown[2]) {
        auto move_delta = (eig::Array3f() 
          << eig::Array2f(io.MouseDelta.x, io.MouseDelta.y) / view_size.array(), 0).finished();
        arcb_handle.getw<detail::Arcball>()
              .set_move_delta(move_delta);
      }
    }
  };
} // namespace met::detail