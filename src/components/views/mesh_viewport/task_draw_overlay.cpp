#include <metameric/core/scene.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_overlay.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/render/primitives_query.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  bool MeshViewportDrawOverlayTask::is_active(SchedulerHandle &info) {
    met_trace();
    const auto &e_query  = info.relative("viewport_input")("path_query").getr<FullPathQueryPrimitive>();
    return !e_query.data().empty();
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
    
  void MeshViewportDrawOverlayTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get handles, shared resources, modified resources
    const auto &e_scene  = info.global("scene").getr<Scene>();
    const auto &e_target = info.relative("viewport_begin")("lrgb_target").getr<gl::Texture2d4f>();
    const auto &e_sensor = info.relative("viewport_render")("sensor").getr<Sensor>();
    const auto &e_render = info.relative("viewport_render")("renderer").getr<detail::IntegrationRenderPrimitive>();
    const auto &e_query  = info.relative("viewport_input")("path_query").getr<FullPathQueryPrimitive>();
    auto &i_target = info("target").getw<gl::Texture2d4f>();

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

    uint n_dispatch = 2 * 4 * static_cast<uint>(e_query.data().size());

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
} // namespace met