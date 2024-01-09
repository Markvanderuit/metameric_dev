#include <metameric/core/scene.hpp>
#include <metameric/components/views/mesh_viewport/task_draw_gbuffer.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/render/renderer.hpp>
#include <metameric/render/sensor.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  bool MeshViewportDrawGBufferTask::is_active(SchedulerHandle &info) {
    met_trace();

    const auto &e_scene   = info.global("scene").getr<Scene>();

    auto is_objc_present = !e_scene.components.objects.empty();
    auto is_objc_updated = e_scene.components.objects.is_mutated();
    auto is_view_present = info.relative("viewport_begin")("is_active").getr<bool>();
    auto is_view_updated = info.relative("viewport_begin")("lrgb_target").is_mutated()
                       ||  info.relative("viewport_input")("arcball").is_mutated();

    return is_objc_present && is_view_present && (is_objc_updated || is_view_updated);
  }

  void MeshViewportDrawGBufferTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Initialize program object
    m_program = {{ .type       = gl::ShaderType::eVertex,
                   .spirv_path = "resources/shaders/views/draw_mesh_gbuffer.vert.spv",
                   .cross_path = "resources/shaders/views/draw_mesh_gbuffer.vert.json" },
                 { .type       = gl::ShaderType::eFragment,
                   .spirv_path = "resources/shaders/views/draw_mesh_gbuffer.frag.spv",
                   .cross_path = "resources/shaders/views/draw_mesh_gbuffer.frag.json" }};

    // Initialize uniform camera buffer and corresponding mapping
    m_unif_buffer     = {{ .size = sizeof(UnifLayout), .flags = buffer_create_flags }};
    m_unif_buffer_map = m_unif_buffer.map_as<UnifLayout>(buffer_access_flags).data();

    // Initialize draw object
    m_draw = { 
      .type             = gl::PrimitiveType::eTriangles,
      .capabilities     = {{ gl::DrawCapability::eDepthTest, true },
                           { gl::DrawCapability::eCullOp,    true }},
      .draw_op          = gl::DrawOp::eFill,
      .bindable_program = &m_program,
    };

    info("gbuffer").set<gl::Texture2d4f>({ });       // packed gbuffer texture
    info("gbuffer_renderer").set<detail::GBuffer>({}); // packed gbuffer texture
    info("gbuffer_sensor").set<Sensor>({}); // packed gbuffer texture
  }
    
  void MeshViewportDrawGBufferTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get handle to relative task resource
    auto target_handle  = info.relative("viewport_begin")("lrgb_target");
    auto arcball_handle = info.relative("viewport_input")("arcball");

    // Get shared resources 
    const auto &e_scene   = info.global("scene").getr<Scene>();
    // const auto &e_objects = e_scene.components.objects;
    // const auto &e_meshes  = e_scene.resources.meshes;
    const auto &e_arcball = info.relative("viewport_input")("arcball").getr<detail::Arcball>();
    const auto &e_target  = target_handle.getr<gl::Texture2d4f>();

    /* // Rebuild framebuffer and g-buffer textures if necessary
    if (!m_fbo.is_init() || (m_fbo_depth.size() != e_target.size()).any()) {
      // Get write-handles to g-buffer textures
      auto &i_gbuffer = info("gbuffer").getw<gl::Texture2d4f>();
      
      // Rebuild depth target and g-buffer textures
      m_fbo_depth = {{ .size = e_target.size().max(1).eval() }};
      i_gbuffer   = {{ .size = e_target.size().max(1).eval() }};

      // Rebuild framebuffer
      m_fbo = {{ .type = gl::FramebufferType::eColor, .attachment = &i_gbuffer   },
               { .type = gl::FramebufferType::eDepth, .attachment = &m_fbo_depth }};
    } */



    // Push camera matrix to uniform data
    if (target_handle.is_mutated() || arcball_handle.is_mutated()) {
      /* m_unif_buffer_map->trf = e_arcball.full().matrix();
      m_unif_buffer.flush(); */
    }

    
    auto &i_sensor  = info("gbuffer_sensor").getw<Sensor>();
    auto &i_gbuffer = info("gbuffer_renderer").getw<detail::GBuffer>();

    i_sensor.film_size = e_target.size();
    i_sensor.proj_trf  = e_arcball.proj().matrix();
    i_sensor.view_trf  = e_arcball.view().matrix();
    i_sensor.flush();
    
    // Forward to gbuffer draw
    i_gbuffer.render(i_sensor, e_scene);

    /* // Assemble appropriate draw data for each object in the scene
    m_draw.bindable_array = &e_scene.resources.meshes.gl.array;
    m_draw.commands.resize(e_objects.size());
    rng::transform(e_objects, m_draw.commands.begin(), [&](const auto &comp) {
      guard(comp.value.is_active, gl::MultiDrawInfo::DrawCommand { });
      return e_meshes.gl.draw_commands[comp.value.mesh_i];
    });

    // Prepare framebuffer state
    m_fbo.bind();
    m_fbo.clear(gl::FramebufferType::eColor, eig::Array4f(0), 0);
    m_fbo.clear(gl::FramebufferType::eDepth, 1.f);
    
    // Bind required resources to their corresponding targets
    m_program.bind("b_buff_unif",    m_unif_buffer);
    m_program.bind("b_buff_objects", e_objects.gl.object_info);
    m_program.bind("b_buff_meshes",  e_meshes.gl.mesh_info);

    // Dispatch draw call to handle entire scene
    gl::sync::memory_barrier(gl::BarrierFlags::eFramebuffer        | 
                             gl::BarrierFlags::eTextureFetch       |
                             gl::BarrierFlags::eClientMappedBuffer |
                             gl::BarrierFlags::eUniformBuffer      );
    gl::dispatch_multidraw(m_draw); */
  }
} // namespace met