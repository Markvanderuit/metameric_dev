#include <metameric/components/views/mmv_viewport/task_draw_mmv.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/array.hpp>
#include <small_gl/texture.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  bool DrawMMVTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info.relative("viewport_begin")("is_active").getr<bool>() &&
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
    met_trace_full();
    
    // Get shared resources
    const auto &e_draw = info.relative("viewport_gen_mmv")("chull_draw").getr<gl::DrawInfo>();
    const auto &e_arcb = info.relative("viewport_camera_input")("arcball").getr<detail::Arcball>();
    const auto &e_trgt = info.relative("viewport_begin")("lrgb_target").getr<gl::Texture2d4f>();

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
    gl::dispatch_draw(e_draw);
  }
} // namespace met