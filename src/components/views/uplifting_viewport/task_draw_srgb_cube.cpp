#include <metameric/components/views/uplifting_viewport/task_draw_srgb_cube.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/texture.hpp>

namespace met {
  bool DrawSRGBCubeTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info.relative("viewport_begin")("is_active").getr<bool>();
  }

  void DrawSRGBCubeTask::init(SchedulerHandle &info) {
    met_trace_full();

    std::vector verts = { eig::AlArray3f { 0, 0, 0 }, eig::AlArray3f { 0, 1, 0 },   // 0, 1
                          eig::AlArray3f { 1, 0, 0 }, eig::AlArray3f { 1, 1, 0 },   // 2, 3
                          eig::AlArray3f { 0, 0, 1 }, eig::AlArray3f { 0, 1, 1 },   // 4, 5
                          eig::AlArray3f { 1, 0, 1 }, eig::AlArray3f { 1, 1, 1 } }; // 6, 7
    std::vector elems = { eig::Array3u { 0, 1, 2 }, eig::Array3u { 1, 2, 3 },
                          eig::Array3u { 4, 5, 6 }, eig::Array3u { 5, 6, 7 },
                          eig::Array3u { 0, 0, 0 }, eig::Array3u { 0, 0, 0 },
                          eig::Array3u { 0, 0, 0 }, eig::Array3u { 0, 0, 0 },
                          eig::Array3u { 0, 0, 0 }, eig::Array3u { 0, 0, 0 },
                          eig::Array3u { 0, 0, 0 }, eig::Array3u { 0, 0, 0 } };

    m_buff_verts = {{ .data = cnt_span<const std::byte>(verts) }};
    m_buff_elems = {{ .data = cnt_span<const std::byte>(elems) }};
    m_array = {{
      .buffers  = {{ .buffer = &m_buff_verts, .index = 0, .stride = sizeof(eig::Array4f) }},
      .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
      .elements = &m_buff_elems
    }};

    // Generate program object
    m_program = {{ .type = gl::ShaderType::eVertex,   
                   .spirv_path = "resources/shaders/views/draw_cube.vert.spv",
                   .cross_path = "resources/shaders/views/draw_cube.vert.json" },
                 { .type = gl::ShaderType::eFragment, 
                   .spirv_path = "resources/shaders/views/draw_cube.frag.spv",
                   .cross_path = "resources/shaders/views/draw_cube.frag.json" }};
  }

  void DrawSRGBCubeTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get handle to the accompanying data generating task
    auto gen_task_name = fmt::format("gen_upliftings.gen_uplifting_{}", m_uplifting_i);
    auto gen_task_info = info.task(gen_task_name).mask(info);

    // Get shared resources
    const auto &e_arcb = info.relative("viewport_camera_input")("arcball").getr<detail::Arcball>();
    const auto &e_trgt = info.relative("viewport_begin")("lrgb_target").getr<gl::Texture2d4f>();

    // Update sensor settings
    m_sensor.proj_trf  = e_arcb.proj().matrix();
    m_sensor.view_trf  = e_arcb.view().matrix();
    m_sensor.film_size = e_trgt.size();
    m_sensor.flush();

    // Set shared OpenGL state for coming draw operations
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eBlendOp, true) };

    // Bind relevant resources
    m_program.bind();
    m_program.bind("b_buff_sensor_info", m_sensor.buffer());

    // Dispatch draw object
    gl::dispatch_draw(gl::DrawInfo {
        .type           = gl::PrimitiveType::eTriangles,
        .vertex_count   = 36u,
        .capabilities   = {{ gl::DrawCapability::eDepthTest, true },
                           { gl::DrawCapability::eCullOp,   false }},
        .draw_op        = gl::DrawOp::eLine,
        .bindable_array = &m_array
    });
  }
} // namespace met