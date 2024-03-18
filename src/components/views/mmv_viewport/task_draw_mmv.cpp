#include <metameric/core/scene.hpp>
#include <metameric/components/views/scene_viewport/task_input_editor.hpp>
#include <metameric/components/views/mmv_viewport/task_draw_mmv.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/array.hpp>
#include <small_gl/texture.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;
  
  void DrawMMVTask::eval_draw_constraints(SchedulerHandle &info) {
    met_trace_full();
    
    // Get handles, shared resources, modified resources
    const auto &e_scene   = info.global("scene").getr<Scene>();
    const auto &e_arcball = info.relative("viewport_camera")("arcball").getr<detail::Arcball>();
    const auto &e_is      = info.parent()("selection").getr<InputSelection>();
    const auto &e_vert    = e_scene.get_uplifting_vertex(e_is.uplifting_i, e_is.constraint_i);

    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui:: GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    
    // Used for coming draw operation
    auto dl = ImGui::GetWindowDrawList();

    std::visit(overloaded {
      [&](const ColorConstraint auto &cstr) {
        // TODO selectable viewed constraint
        auto p = cstr.colr_j[0];

        // Determine window-space position of surface point
        eig::Vector2f p_window = eig::world_to_window_space(p, e_arcball.full(), viewport_offs, viewport_size);
          
        // Clip vertex outside viewport
        guard((p_window.array() >= viewport_offs).all() 
           && (p_window.array() <= viewport_offs + viewport_size).all());

        // Get srgb colors
        auto circle_color_center 
          = ImGui::ColorConvertFloat4ToU32((eig::Vector4f() << lrgb_to_srgb(p), 1.f).finished());
        auto circle_color_border = ImGui::ColorConvertFloat4ToU32({ 1.f, 1.f, 1.f, 1.f });

        // Draw pair of circles with special colors
        dl->AddCircleFilled(p_window, 8.f, circle_color_border);
        dl->AddCircleFilled(p_window, 4.f, circle_color_center);
      },
      [](const auto &) { /* ... */ }
    }, e_vert.constraint);
  }

  void DrawMMVTask::eval_draw_volume(SchedulerHandle &info) {
    met_trace_full();
    
    // Get shared resources
    const auto &e_draw_chull  = info.relative("viewport_gen_mmv")("chull_draw").getr<gl::DrawInfo>();
    const auto &e_draw_points = info.relative("viewport_gen_mmv")("points_draw").getr<gl::DrawInfo>();
    const auto &e_arcb        = info.relative("viewport_camera")("arcball").getr<detail::Arcball>();
    const auto &e_trgt        = info.relative("viewport_image")("lrgb_target").getr<gl::Texture2d4f>();

    // Update sensor settings
    m_sensor.proj_trf  = e_arcb.proj().matrix();
    m_sensor.view_trf  = e_arcb.view().matrix();
    m_sensor.film_size = e_trgt.size();
    m_sensor.flush();

    // Bind relevant resources
    m_program.bind();
    m_program.bind("b_buff_sensor",   m_sensor.buffer());
    m_program.bind("b_buff_settings", m_unif_buffer);

    // Prepare draw state
    gl::state::set_depth_range(0.f, 1.f);
    gl::state::set_viewport(e_trgt.size());
    gl::state::set_op(gl::CullOp::eBack);
    gl::state::set_op(gl::DepthOp::eLessOrEqual);
    gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eBlendOp, true) };
    
    // Dispatch draw information
    gl::dispatch_draw(e_draw_chull);
    gl::dispatch_draw(e_draw_points);
  }

  bool DrawMMVTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info.parent()("is_active").getr<bool>() &&
           info.relative("viewport_gen_mmv")("chull_array").getr<gl::Array>().is_init();
  }

  void DrawMMVTask::init(SchedulerHandle &info) {
    met_trace_full();
    
    // Generate program object
    m_program = {{ .type       = gl::ShaderType::eVertex,   
                   .spirv_path = "resources/shaders/views/mmv_viewport/draw_mmv_hull.vert.spv",
                   .cross_path = "resources/shaders/views/mmv_viewport/draw_mmv_hull.vert.json" },
                 { .type       = gl::ShaderType::eFragment, 
                   .spirv_path = "resources/shaders/views/mmv_viewport/draw_mmv_hull.frag.spv",
                   .cross_path = "resources/shaders/views/mmv_viewport/draw_mmv_hull.frag.json" }};
    
    // Generate and set mapped uniform buffer
    m_unif_buffer     = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
    m_unif_buffer_map = m_unif_buffer.map_as<UnifLayout>(buffer_access_flags).data();
    m_unif_buffer_map->alpha = 1.f;
  }

  void DrawMMVTask::eval(SchedulerHandle &info) {
    eval_draw_volume(info);
    eval_draw_constraints(info);
  }
} // namespace met