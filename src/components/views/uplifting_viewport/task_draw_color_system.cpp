#include <metameric/components/views/uplifting_viewport/task_draw_color_system.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/texture.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  bool DrawColorSystemTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info.relative("viewport_begin")("is_active").getr<bool>();
  }

  void DrawColorSystemTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Generate program object
    m_program = {{ .type = gl::ShaderType::eVertex,   
                   .spirv_path = "resources/shaders/views/uplifting_viewport/draw_color_system.vert.spv",
                   .cross_path = "resources/shaders/views/uplifting_viewport/draw_color_system.vert.json" },
                 { .type = gl::ShaderType::eFragment, 
                   .spirv_path = "resources/shaders/views/uplifting_viewport/draw_color_system.frag.spv",
                   .cross_path = "resources/shaders/views/uplifting_viewport/draw_color_system.frag.json" }};
   
    // Generate and set mapped uniform buffers
    m_unif_settings     = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
    m_unif_settings_map = m_unif_settings.map_as<UnifLayout>(buffer_access_flags).data();

    // Set uniform alpha settings for now
    m_unif_settings_map->alpha = 1.f;
  }

  void DrawColorSystemTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get handle to the accompanying data generating task
    auto gen_task_name = std::format("gen_upliftings.gen_uplifting_{}", m_uplifting_i);
    auto gen_task_info = info.task(gen_task_name).mask(info);

    // Get shared resources
    const auto &e_arcb = info.relative("viewport_camera_input")("arcball").getr<detail::Arcball>();
    const auto &e_trgt = info.relative("viewport_begin")("lrgb_target").getr<gl::Texture2d4f>();
    const auto &e_draw = gen_task_info("tesselation_draw").getr<gl::DrawInfo>();

    // Update sensor settings
    m_sensor.proj_trf  = e_arcb.proj().matrix();
    m_sensor.view_trf  = e_arcb.view().matrix();
    m_sensor.film_size = e_trgt.size();
    m_sensor.flush();

    // Set shared OpenGL state for coming draw operations
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eBlendOp, true) };

    // Bind relevant resources
    m_program.bind();
    m_program.bind("b_buff_settings", m_unif_settings);
    m_program.bind("b_buff_sensor",   m_sensor.buffer());

    // Dispatch draw object
    gl::dispatch_draw(e_draw);
  }
} // namespace met