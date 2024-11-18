#include <metameric/core/ranges.hpp>
#include <metameric/scene/scene.hpp>
#include <metameric/components/views/scene_viewport/task_draw_overlay.hpp>
#include <metameric/components/views/scene_viewport/task_input_editor.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/render/primitives_query.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  static const auto inforect_color_black = ImGui::ColorConvertFloat4ToU32({ 0.f, 0.f, 0.f, .35f });
  static const auto vertex_color_white   = ImGui::ColorConvertFloat4ToU32({ 1.f, 1.f, 1.f, 1.f  });
  static const auto vertex_color_valid   = ImGui::ColorConvertFloat4ToU32({ .5f, .5f, 1.f, 1.f  });
  static const auto vertex_color_invalid = ImGui::ColorConvertFloat4ToU32({ 1.f, .5f, .5f, 1.f  });

  bool MeshViewportDrawOverlayTask::is_active(SchedulerHandle &info) {
    return info.parent()("is_active").getr<bool>();
  }
  
  void MeshViewportDrawOverlayTask::init(SchedulerHandle &info) {
    met_trace_full();

      // Initialize program object
      m_program = {{ .type       = gl::ShaderType::eVertex,
                     .spirv_path = "resources/shaders/views/draw_paths.vert.spv",
                     .cross_path = "resources/shaders/views/draw_paths.vert.json" },
                   { .type       = gl::ShaderType::eFragment,
                     .spirv_path = "resources/shaders/views/draw_paths.frag.spv",
                     .cross_path = "resources/shaders/views/draw_paths.frag.json" }};

    // Initialize output texture
    info("target").init<gl::Texture2d4f>({ .size = eig::Array2u(1) });

    // Initialize bare VAO; we draw data from SSBO
    m_vao = {{ }};
  }
  
  // Helper method to draw all active surface constraints in the scene
  void MeshViewportDrawOverlayTask::eval_draw_constraints(SchedulerHandle &info) {
    met_trace_full();
    
    // Get handles, shared resources, modified resources
    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &e_arcball = info.relative("viewport_input_camera")("arcball").getr<detail::Arcball>();
    const auto &e_active_constraints 
                          = info.relative("viewport_input_editor")("active_constraints").getr<std::vector<ConstraintRecord>>();
    
    // Useful for coming operations
    auto dl = ImGui::GetWindowDrawList();

    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

    // Generate view over surface points that are stored in all active scene constraints
    auto all_surfaces 
      = e_active_constraints
      | vws::transform([&](ConstraintRecord cs) { return e_scene.uplifting_vertex(cs); })
      | vws::transform([](const auto &v) { return v.surfaces(); })
      | vws::join;

    // Generate view over surface points that are "free variables" in all active scene constraints
    auto active_surfaces
      = e_active_constraints
      | vws::transform([&](ConstraintRecord cs) { return e_scene.uplifting_vertex(cs); })
      | vws::transform([](const auto &v) { return v.surface(); });

    // Draw vertex for each vertex at its surface
    for (const auto &si : all_surfaces) {
      // Get window-space position of surface point, and of
      // a slightly offset point along the surface normal
      eig::Vector2f p_window = eig::world_to_window_space(si.p, e_arcball.full(), viewport_offs, viewport_size);

      // Clip vertices outside viewport
      guard_continue((p_window.array() >= viewport_offs).all() && (p_window.array() <= viewport_offs + viewport_size).all());

      // Get srgb vertex color;
      auto vertex_color_si = ImGui::ColorConvertFloat4ToU32((eig::Vector4f() << lrgb_to_srgb(si.diffuse), 1.f).finished());
      
      // Vertex color is obtained from surface diffuse color
      auto vertex_color_border = si.is_valid() ? vertex_color_valid : vertex_color_invalid;
      auto vertex_color_center = si.is_valid() ? vertex_color_si    : vertex_color_white;

      // Draw vertex with special coloring dependent on constraint state
      dl->AddCircleFilled(p_window, 4.f, vertex_color_border);
      dl->AddCircleFilled(p_window, 2.f, vertex_color_center);
    }

    // Draw vertex for each visible vertex at its surface
    for (const SurfaceInfo &si : active_surfaces) {
      // Get window-space position of surface point, and of
      // a slightly offset point along the surface normal
      eig::Vector2f p_window = eig::world_to_window_space(si.p, e_arcball.full(), viewport_offs, viewport_size);
      eig::Vector2f n_window = eig::world_to_window_space(si.p + si.n * 0.025f, e_arcball.full(), viewport_offs, viewport_size);

      // Clip vertices outside viewport
      guard_continue((p_window.array() >= viewport_offs).all() && (p_window.array() <= viewport_offs + viewport_size).all());

      // Get srgb vertex color;
      auto vertex_color_si = ImGui::ColorConvertFloat4ToU32((eig::Vector4f() << lrgb_to_srgb(si.diffuse), 1.f).finished());

      // Vertex color is obtained from surface diffuse color
      auto vertex_color_border = si.is_valid() ? vertex_color_valid : vertex_color_invalid;
      auto vertex_color_center = si.is_valid() ? vertex_color_si    : vertex_color_white;

      // Draw vertex with special coloring dependent on constraint state
      // and a small line along the geometric normal with a dot on the end
      dl->AddCircleFilled(p_window, 8.f, vertex_color_border);
      if (si.is_valid()) {
        dl->AddCircleFilled(n_window, 6.f, vertex_color_border);
        dl->AddLine(p_window, n_window, vertex_color_border, 6.f);
      }
      dl->AddCircleFilled(p_window, 4.f, vertex_color_center);
    }
  }

  // Helper method to draw render view rect
  void MeshViewportDrawOverlayTask::eval_draw_frustrum(SchedulerHandle &info) {
    met_trace_full();

    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

    // Get external resources
    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &e_arcball = info.relative("viewport_input_camera")("arcball").getr<detail::Arcball>();

    // Iterate views with the draw_frustrum flag enabled
    for (const auto [e_view, _] : e_scene.components.views) {
      guard_continue(e_view.draw_frustrum);

      // Transform a view quad into world space
      eig::Projective3f full_trf;
      {
        eig::Affine3f trf_rot = eig::Affine3f::Identity();
        trf_rot *= eig::AngleAxisf(e_view.camera_trf.rotation.x(), eig::Vector3f::UnitY());
        trf_rot *= eig::AngleAxisf(e_view.camera_trf.rotation.y(), eig::Vector3f::UnitX());
        trf_rot *= eig::AngleAxisf(e_view.camera_trf.rotation.z(), eig::Vector3f::UnitZ());

        auto dir = (trf_rot * eig::Vector3f(0, 0, 1)).normalized().eval();
        auto eye = -dir; 
        auto cen = (e_view.camera_trf.position + dir).eval();
        auto ab = e_arcball;

        ab.set_zoom(1);
        ab.set_fov_y(e_view.camera_fov_y * std::numbers::pi_v<float> / 180.f);
        ab.set_eye(eye);
        ab.set_center(cen);
        ab.set_aspect(static_cast<float>(e_view.film_size.x()) / static_cast<float>(e_view.film_size.y()));

        full_trf = ab.full();
      }

      std::vector quad = { eig::Array4f(0.f, 0.f, 1, 1), eig::Array4f(1.f, 0.f, 1, 1),
                          eig::Array4f(1.f, 1.f, 1, 1), eig::Array4f(0.f, 1.f, 1, 1) };

      // Determine rectangle of points around the image plane in world space
      auto image_world = quad | vws::transform([full_trf](const auto &v) { 
        return eig::screen_to_world_space(v.template head<2>().eval(), full_trf);
      }) | view_to<std::vector<eig::Vector3f>>();
      
      // Determine second rectangle of points, offset into camera frustrum
      auto frust_world = image_world | vws::transform([center = e_view.camera_trf.position](const auto &v) {
        return (center + 0.25 * (v - center).matrix().normalized()).array().eval();
      }) | view_to<std::vector<eig::Vector3f>>();

      // Compute window-space representations
      auto image_window = image_world | vws::transform([&](const auto &v) {
        auto trf = (e_arcball.full() * (eig::Vector4f() << v, 1).finished()).array().eval();
        trf /= trf.w();
        if (trf.z() > 1.f)
          return eig::Vector2f(0);
        else
          return eig::screen_to_window_space(trf.template head<2>() * .5f + .5f, viewport_offs, viewport_size);
      }) | view_to<std::vector<eig::Vector2f>>();
      auto frust_window = frust_world | vws::transform([&](const auto &v) {
        auto trf = (e_arcball.full() * (eig::Vector4f() << v, 1).finished()).array().eval();
        trf /= trf.w();
        if (trf.z() > 1.f)
          return eig::Vector2f(0);
        else
          return eig::screen_to_window_space(trf.template head<2>() * .5f + .5f, viewport_offs, viewport_size);
      }) | view_to<std::vector<eig::Vector2f>>();

      // Draw frustrum
      auto dl = ImGui::GetWindowDrawList();
      dl->AddQuad(image_window[0], image_window[1], image_window[2], image_window[3], vertex_color_white, 1.f);
      dl->AddQuad(frust_window[0], frust_window[1], frust_window[2], frust_window[3], vertex_color_white, 1.f);
      for (uint i = 0; i < 4; ++i) {
        guard_continue(!image_window[i].isZero() && !frust_window[i].isZero());
        dl->AddLine(image_window[i], frust_window[i], vertex_color_white, 1.f);
      }
    } // for (e_view)
  }

  void MeshViewportDrawOverlayTask::eval_draw_info(SchedulerHandle &info) {
    met_trace();

    // Get handles, shared resources, modified resources
    const auto &e_scene  = info.global("scene").getr<Scene>();
    const auto &e_render = info.relative("viewport_render")("renderer").getr<detail::IntegrationRenderPrimitive>();
    
    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

    auto dl = ImGui::GetWindowDrawList();
    
    auto intr_offs = eig::Array2f(4.f);
    auto view_offs = (viewport_offs + intr_offs).eval();
    auto view_size = eig::Array2f(160, 42);
    
    auto text = fmt::format("scene: {}\nres: {} x {}\nspp: {}", 
      e_scene.save_path.filename().string(),
      e_render.film().size().x(), 
      e_render.film().size().y(), 
      e_render.spp_curr() + 1);

    dl->AddRectFilled(view_offs,
                      (view_offs + view_size).eval(),
                      inforect_color_black, 
                      1.f, 
                      ImDrawFlags_RoundCornersAll);
    dl->AddText(ImGui::GetFont(), 
                12.f,
                (view_offs + intr_offs).eval(),
                vertex_color_white,
                text.c_str(),
                nullptr,
                view_size.x() - intr_offs.x());    
  } 

  // Helper method to draw all paths generated by path query for now
  void MeshViewportDrawOverlayTask::eval_draw_path_queries(SchedulerHandle &info) {
    met_trace_full();

    // Escape early if query task d.n.e.
    guard(info.relative_task("viewport_input_query").is_init());

    // Get handles, shared resources, modified resources
    const auto &e_scene  = info.global("scene").getr<Scene>();
    const auto &e_target = info.relative("viewport_image")("lrgb_target").getr<gl::Texture2d4f>();
    const auto &e_sensor = info.relative("viewport_render")("sensor").getr<Sensor>();
    const auto &e_render = info.relative("viewport_render")("renderer").getr<detail::IntegrationRenderPrimitive>();
    const auto &e_query  = info.relative("viewport_input_query")("path_query").getr<PathQueryPrimitive>();
    auto &i_target       = info("target").getw<gl::Texture2d4f>();


    // Prepare output framebuffer
    if (!i_target.is_init() || !i_target.size().isApprox(e_target.size())) {
      i_target = {{ .size = e_target.size() }};
      m_dbo    = {{ .size = e_target.size() }};
      m_fbo    = {{ .type = gl::FramebufferType::eColor, .attachment = &i_target },
                  { .type = gl::FramebufferType::eDepth, .attachment = &m_dbo    }};
    }

    // Clear framebuffer targets
    eig::Array4f fbo_colr_value = { 0, 0, 0, 0 };
    m_fbo.bind();
    m_fbo.clear(gl::FramebufferType::eColor, fbo_colr_value, 0);
    m_fbo.clear(gl::FramebufferType::eDepth, 1.f);

    // Return instead of performing draw
    guard(!e_query.data().empty());
    
    // Prepare draw state
    gl::state::set_viewport(i_target.size());    
    gl::state::set_depth_range(0.f, 1.f);
    gl::state::set_op(gl::DepthOp::eLessOrEqual);
    gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);

    // Prepare binding state
    m_fbo.bind();
    m_vao.bind();
    m_program.bind();
    m_program.bind("b_buff_sensor_info", e_sensor.buffer());
    m_program.bind("b_buff_paths",  e_query.output());
    m_program.bind("b_cmfs_3f",     e_scene.resources.observers.gl.cmfs_texture);

    uint n_dispatch = 2 
                    * PathRecord::path_max_depth
                    * static_cast<uint>(e_query.data().size());

    // Dispatch draw call
    gl::sync::memory_barrier(gl::BarrierFlags::eFramebuffer   | 
                            gl::BarrierFlags::eUniformBuffer );
    gl::dispatch_draw({ .type                 = gl::PrimitiveType::eLines,
                        .vertex_count         = n_dispatch,
                        .capabilities         = {{ gl::DrawCapability::eDepthTest, false },
                                                 { gl::DrawCapability::eBlendOp,   true  },
                                                 { gl::DrawCapability::eCullOp,    false }},
                        .bindable_array       = &m_vao,
                        .bindable_program     = &m_program,
                        .bindable_framebuffer = &m_fbo });
  }

  void MeshViewportDrawOverlayTask::eval(SchedulerHandle &info) {
    met_trace_full();
    eval_draw_path_queries(info);
    eval_draw_constraints(info);
    eval_draw_frustrum(info);
    eval_draw_info(info);
  }
} // namespace met