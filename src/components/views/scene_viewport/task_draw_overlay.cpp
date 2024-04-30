#include <metameric/core/ranges.hpp>
#include <metameric/core/scene.hpp>
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
  static const auto  vertex_color_white   = ImGui::ColorConvertFloat4ToU32({ 1.f, 1.f, 1.f, 1.f });
  static const auto  vertex_color_valid   = ImGui::ColorConvertFloat4ToU32({ .5f, .5f, 1.f, 1.f });
  static const auto  vertex_color_invalid = ImGui::ColorConvertFloat4ToU32({ 1.f, .5f, .5f, 1.f });
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

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
      | vws::join
      | rng::to<std::vector>();

    // Generate view over surface points that are "free variables" in all active scene constraints
    auto active_surfaces
      = e_active_constraints
      | vws::transform([&](ConstraintRecord cs) { return e_scene.uplifting_vertex(cs); })
      | vws::transform([](const auto &v) { return v.surface(); })
      | rng::to<std::vector>();

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
  void MeshViewportDrawOverlayTask::eval_draw_view_rect(SchedulerHandle &info) {
    met_trace_full();

    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &e_view_i  = info.parent()("view_settings_i").getr<uint>();
    const auto &e_arcball = info.relative("viewport_input_camera")("arcball").getr<detail::Arcball>();
    const auto &e_view    = e_scene.components.views[e_view_i].value;
    
    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

    // Transform a view quad into world space
    auto aspect   = static_cast<float>(e_view.film_size.x()) / static_cast<float>(e_view.film_size.y());
    auto view_trf = e_view.camera_trf.affine().matrix();
    std::vector quad = { eig::Array4f(-.5 * aspect, -.5, 1, 1),
                         eig::Array4f(-.5 * aspect,  .5, 1, 1),
                         eig::Array4f( .5 * aspect,  .5, 1, 1),
                         eig::Array4f( .5 * aspect, -.5, 1, 1) };
    std::vector<eig::Vector2f> p_window(4);
    rng::transform(quad, p_window.begin(), [&](const auto &v) {
      auto p = ((view_trf) * v.matrix()).head<3>().eval();
      return eig::world_to_window_space(p, e_arcball.full(), viewport_offs, viewport_size);
    });
    
    // Draw quad
    auto dl = ImGui::GetWindowDrawList();
    dl->AddQuad(p_window[0], p_window[1], p_window[2], p_window[3], vertex_color_white, 1.f);
    for (const auto &p : p_window)
      dl->AddCircleFilled(p, 2.f, vertex_color_white);
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

    guard(!e_query.data().empty());

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

    // Prepare draw state
    gl::state::set_viewport(i_target.size());    
    gl::state::set_depth_range(0.f, 1.f);
    gl::state::set_op(gl::DepthOp::eLessOrEqual);
    gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);

    // Prepare binding state
    m_fbo.bind();
    m_vao.bind();
    m_program.bind();
    m_program.bind("b_buff_sensor",     e_sensor.buffer());
    m_program.bind("b_buff_paths",      e_query.output());
    m_program.bind("b_buff_wvls_distr", e_scene.components.colr_systems.gl.wavelength_distr_buffer);
    m_program.bind("b_cmfs_3f",         e_scene.resources.observers.gl.cmfs_texture);

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
    eval_draw_constraints(info);
    eval_draw_path_queries(info);
    eval_draw_view_rect(info);
  }
} // namespace met