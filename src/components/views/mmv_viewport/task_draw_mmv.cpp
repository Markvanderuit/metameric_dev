#include <metameric/core/ranges.hpp>
#include <metameric/scene/scene.hpp>
#include <metameric/components/views/scene_viewport/task_input_editor.hpp>
#include <metameric/components/views/mmv_viewport/task_draw_mmv.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/array.hpp>
#include <small_gl/texture.hpp>

namespace met {
  void DrawMMVTask::eval_draw_constraint(SchedulerHandle &info) {
    met_trace();
    
    // Get handles, shared resources, modified resources
    const auto &e_scene         = info.global("scene").getr<Scene>();
    const auto &e_vert          = e_scene.uplifting_vertex(info.parent()("selection").getr<ConstraintRecord>());
    const auto &e_arcball       = info.relative("viewport_camera")("arcball").getr<detail::Arcball>();
    const auto &e_gizmo_active  = info.relative("viewport_guizmo")("is_active").getr<bool>();
    const auto &e_closest_point = info.relative("viewport_guizmo")("closest_point").getr<Colr>();
    const auto &e_trnf          = info.relative("viewport_gen_mmv")("chull_trnf").getr<eig::Matrix4f>();
    const auto &io              = ImGui::GetIO();

    // Get [0, 1] matrix and inverse, as the displayed mesh is scaled
    auto proj     = [m = e_trnf.inverse().eval()](eig::Array3f p) -> Colr { return (m * (eig::Vector4f() << p, 1).finished()).head<3>(); };
    auto proj_inv = [m = e_trnf](eig::Array3f p)                  -> Colr { return (m * (eig::Vector4f() << p, 1).finished()).head<3>(); };

    // Compute viewport offset and size, minus ImGui's tab bars etc
    eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                               + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                               - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
    
    // Used for coming draw operation
    auto dl = ImGui::GetWindowDrawList();

    // Visit underlying color constraint to extract edit position, then
    // determine window-space position of surface point
    auto p        = e_vert.get_mismatch_position();
    auto p_mouse  = eig::Array2f(io.MousePos);
    auto p_window = eig::world_to_window_space(proj(p), e_arcball.full(), viewport_offs, viewport_size);
      
    // Clip vertex outside viewport
    guard((p_window.array() >= viewport_offs).all() 
       && (p_window.array() <= viewport_offs + viewport_size).all());

    // If closest-projected-point information is available, and the gizmo is being dragged,
    // draw a helper line towards it
    if (e_gizmo_active) {
      auto closest_colr_line = ImGui::ColorConvertFloat4ToU32({ 1.f, 1.f, 1.f, 1.f });
      dl->AddLine(p_window, p_mouse, closest_colr_line, 2.f);
    }

    // Get srgb colors for constraint
    auto circle_colr_border = ImGui::ColorConvertFloat4ToU32({ 1.f, 1.f, 1.f, 1.f });
    auto circle_colr_center = ImGui::ColorConvertFloat4ToU32((eig::Vector4f() << lrgb_to_srgb(p), 1.f).finished());

    // Draw pair of circles with special colors
    dl->AddCircleFilled(p_window, 8.f, circle_colr_border);
    dl->AddCircleFilled(p_window, 4.f, circle_colr_center);
  }

  void DrawMMVTask::eval_draw_volume(SchedulerHandle &info) {
    met_trace_full();
    
    // Get shared resources
    const auto &e_draw_chull = info.relative("viewport_gen_mmv")("chull_draw").getr<gl::DrawInfo>();
    const auto &e_arcb       = info.relative("viewport_camera")("arcball").getr<detail::Arcball>();
    const auto &e_trgt       = info.relative("viewport_image")("lrgb_target").getr<gl::Texture2d4f>();

    // Update sensor settings
    m_sensor.proj_trf  = e_arcb.proj().matrix();
    m_sensor.view_trf  = e_arcb.view().matrix();
    m_sensor.film_size = e_trgt.size();
    m_sensor.flush();

    // Bind relevant resources
    m_program.bind();
    m_program.bind("b_buff_sensor_info",   m_sensor.buffer());
    m_program.bind("b_buff_settings", m_unif_buffer);

    // Prepare draw state
    gl::state::set_depth_range(0.f, 1.f);
    gl::state::set_viewport(e_trgt.size());
    gl::state::set_op(gl::CullOp::eBack);
    gl::state::set_op(gl::DepthOp::eLessOrEqual);
    gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
    
    // Dispatch draw information; draw twice, once with fill, then with lines overlaying these
    auto draw = e_draw_chull;
    {
      auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true), 
                                 gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true) };
      draw.draw_op = gl::DrawOp::eFill;
      gl::dispatch_draw(draw);
    }
    {
      auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true), 
                                 gl::state::ScopedSet(gl::DrawCapability::eDepthTest, false) };
      draw.draw_op = gl::DrawOp::eLine;
      gl::dispatch_draw(draw);
    }
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
    std::tie(m_unif_buffer, m_unif_buffer_map) = gl::Buffer::make_flusheable_object<UnifLayout>();
    m_unif_buffer_map->alpha = 1.f;
  }

  void DrawMMVTask::eval(SchedulerHandle &info) {
    eval_draw_volume(info);
    eval_draw_constraint(info);
  }
} // namespace met